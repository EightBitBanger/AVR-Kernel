
#include <kernel/dwm/windows/filecopy.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/kernel.h>
#include <kernel/events.h>
#include <kernel/mutex.h>
#include <kernel/scheduler/scheduler.h>
#include <kernel/memory/malloc.h>
#include <kernel/vfs/vfs.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/string.h>

// ==========================================
// Locking rules for the copy thread
// ==========================================
//
// kernel_big_lock guards the heap and every DWM structure, including the
// job list below. The copy thread holds it for:
//   - malloc / free, and VFS calls that allocate (vfs_open / vfs_close)
//   - directory work (mkdir, listing, remove, rename checks)
//   - anything touching windows, icons or the job list
// and releases it for the slow, heap-free disk calls:
//   - vfs_truncate (reserving space), vfs_seek, vfs_read, vfs_write
// Those take only the VFS lock, which comes after kernel_big_lock in the
// lock order, so calling them without the big lock is allowed.

// ==========================================
// Layout (client coordinates, 6x8 font)
// ==========================================
#define FC_WIDTH           320
#define FC_HEIGHT          130     // Includes the 20px titlebar
#define FC_PAD             15
#define FC_STATUS_Y        12
#define FC_NAME_Y          26
#define FC_BAR_Y           46
#define FC_BAR_H           14
#define FC_PERCENT_Y       (FC_BAR_Y + FC_BAR_H + 8)
#define FC_BUTTON_W        64
#define FC_BUTTON_H        24
#define FC_FONT_W          6
#define FC_FONT_H          8
#define FC_KEY_ESCAPE      0x1B

#define FC_MAX_DEPTH       8       // Folder nesting the copier will follow
#define FC_MAX_ATTEMPTS    99      // "name (2)" ... "name (99)"
#define FC_CELL_W          90      // Desktop icon grid (same spacing as boot.c)
#define FC_CELL_H          80
#define FC_GRID_ORIGIN     30

typedef enum {
    FC_PHASE_WAITING,              // Queued behind another window
    FC_PHASE_PREPARING,            // Reserving space for the current file
    FC_PHASE_COPYING,
    FC_PHASE_VERIFYING,
} FileCopyPhase;

struct FileCopyItem {
    struct FileCopyItem* next;
    char src[FILECOPY_PATH_MAX];
    char dest[FILECOPY_PATH_MAX];
};

struct FileCopyJob {
    struct FileCopyJob*  next;
    WindowHandle         window;
    uint32_t             flags;

    struct FileCopyItem* items_head;
    struct FileCopyItem* items_tail;
    uint32_t             item_count;
    uint32_t             item_index;        // 1-based item being worked on

    // Shown in the window
    FileCopyPhase        phase;
    char                 name[VFS_NAME_MAX + 1];
    uint32_t             done;
    uint32_t             size;
    int                  drawn_percent;
    bool                 cancel;

    // Desktop icon placement
    int                  desktop_x;
    int                  desktop_y;
    bool                 used_origin;

    FileCopyDoneCallback done_callback;
    void*                done_user;

    // Copy thread only
    FileCopyResult       result;
    const char*          last_error;
    uint8_t*             buffer;
    uint8_t*             verify_buffer;
};

// All guarded by kernel_big_lock
static mutex_t*            fc_big_lock = NULL;
static ThreadBlock*        fc_thread   = NULL;
static Event               fc_event    = EVENT_INITIALIZER;
static struct FileCopyJob* fc_jobs     = NULL;    // FIFO, head is running

// ==========================================
// Small helpers
// ==========================================

static size_t fc_u32_to_str(uint32_t value, char* out) {
    char digits[11];
    size_t n = 0;
    do {
        digits[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    for (size_t i = 0; i < n; i++) out[i] = digits[n - 1 - i];
    out[n] = '\0';
    return n;
}

static const char* fc_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}

static void fc_parent(const char* path, char* out, size_t size) {
    memset(out, '\0', size);
    strncpy(out, path, size - 1);

    char* slash = strrchr(out, '/');
    if (slash == NULL)      out[0] = '\0';
    else if (slash == out)  out[1] = '\0';
    else                    *slash = '\0';
}

static bool fc_join(char* out, size_t size, const char* dir, const char* name) {
    size_t dir_len  = strlen(dir);
    size_t name_len = strlen(name);
    bool   slash    = (dir_len == 0 || dir[dir_len - 1] != '/');

    if (dir_len + (slash ? 1 : 0) + name_len + 1 > size) return false;

    memcpy(out, dir, dir_len);
    if (slash) out[dir_len++] = '/';
    memcpy(&out[dir_len], name, name_len + 1);
    return true;
}

// True when `path` is `base` itself or somewhere below it
static bool fc_path_is_within(const char* path, const char* base) {
    size_t base_len = strlen(base);
    while (base_len > 1 && base[base_len - 1] == '/') base_len--;

    if (strncmp(path, base, base_len) != 0) return false;
    return path[base_len] == '\0' || path[base_len] == '/';
}

static struct FileCopyJob* fc_find(WindowHandle handle) {
    for (struct FileCopyJob* job = fc_jobs; job != NULL; job = job->next) {
        if (job->window == handle) return job;
    }
    return NULL;
}

// ==========================================
// Shared helpers (declared in dwm_core_internal.h)
// ==========================================

// Name for attempt n: n == 1 is the name itself, n >= 2 is "stem (n).ext",
// with the stem shortened so the result fits VFS_NAME_MAX. Short extensions
// (".txt") are kept, longer ones are treated as part of the stem.
static void fc_candidate_name(const char* name, uint32_t n, char* out /* VFS_NAME_MAX + 1 */) {
    size_t len = strnlen(name, VFS_NAME_MAX);

    if (n <= 1) {
        memcpy(out, name, len);
        out[len] = '\0';
        return;
    }

    char suffix[8] = " (";
    size_t suffix_len = 2 + fc_u32_to_str(n, &suffix[2]);
    suffix[suffix_len++] = ')';
    suffix[suffix_len] = '\0';

    const char* dot = strrchr(name, '.');
    size_t ext_len = (dot != NULL && dot != name) ? strnlen(dot, VFS_NAME_MAX) : 0;
    if (ext_len > 5 || ext_len >= len) ext_len = 0;

    size_t stem_len = len - ext_len;
    size_t room = VFS_NAME_MAX - suffix_len - ext_len;
    if (stem_len > room) stem_len = room;
    while (stem_len > 1 && name[stem_len - 1] == ' ') stem_len--;   // No "abc  (2)"

    size_t o = 0;
    memcpy(&out[o], name, stem_len);                o += stem_len;
    memcpy(&out[o], suffix, suffix_len);            o += suffix_len;
    memcpy(&out[o], &name[len - ext_len], ext_len); o += ext_len;
    out[o] = '\0';
}

bool dwm_path_unique(const char* dir, const char* name, char* out_path, size_t path_size, char* out_name) {
    for (uint32_t n = 1; n <= FC_MAX_ATTEMPTS; n++) {
        fc_candidate_name(name, n, out_name);
        if (!fc_join(out_path, path_size, dir, out_name)) return false;
        if (!vfs_exists(out_path)) return true;
    }
    return false;
}

bool dwm_desktop_get_directory(char* out, size_t size) {
    struct LocalPaths paths;
    kernel_get_local_paths(&paths);
    if (paths.home[0] == '\0') return false;

    memset(out, '\0', size);
    strncpy(out, paths.home, size - 1);
    strncat(out, "/usr/desktop", size - strlen(out) - 1);
    return true;
}

static bool fc_cell_is_free(int x, int y) {
    for (struct list_node* node = workspace.icon_head; node != NULL; node = node->next) {
        struct IconObject* icon = (struct IconObject*)node->data;
        if ((int)icon->x < x + FC_CELL_W && (int)icon->x + FC_CELL_W > x &&
            (int)icon->y < y + FC_CELL_H && (int)icon->y + FC_CELL_H > y)
            return false;
    }
    return true;
}

void dwm_desktop_add_item_icon(const char* path, const char* name, int x, int y) {
    int max_x = display_get_width()  - FC_CELL_W;
    int max_y = display_get_height() - taskbar.height - FC_CELL_H;

    if (x >= 0 && y >= 0) {
        // Requested spot, kept on screen above the taskbar
        if (x > max_x) x = max_x;
        if (y > max_y) y = max_y;
    } else {
        // Next free grid cell, row by row like the boot-time layout
        x = FC_GRID_ORIGIN;
        y = FC_GRID_ORIGIN;
        bool found = false;
        for (int cy = FC_GRID_ORIGIN; cy <= max_y && !found; cy += FC_CELL_H) {
            for (int cx = FC_GRID_ORIGIN; cx <= max_x; cx += FC_CELL_W) {
                if (fc_cell_is_free(cx, cy)) {
                    x = cx;
                    y = cy;
                    found = true;
                    break;
                }
            }
        }
    }

    if (vfs_directory_check(path)) {
        dwm_create_folder((uint16_t)x, (uint16_t)y, name, path);
    } else {
        dwm_create_file((uint16_t)x, (uint16_t)y, name, path);
    }
}

void dwm_desktop_forget_path(const char* removed_path) {
    struct list_node* node = workspace.icon_head;
    while (node != NULL) {
        struct list_node* next = node->next;   // dwm_destroy_icon frees `node`
        struct IconObject* icon = (struct IconObject*)node->data;

        if (icon != NULL && fc_path_is_within(icon->path, removed_path))
            dwm_destroy_icon(icon);

        node = next;
    }
}

// ==========================================
// Window updates (big lock held)
// ==========================================

static int fc_percent(const struct FileCopyJob* job) {
    if (job->size == 0) return (job->phase == FC_PHASE_COPYING || job->phase == FC_PHASE_VERIFYING) ? 100 : 0;
    int percent = (int)(((uint64_t)job->done * 100) / job->size);
    return (percent > 100) ? 100 : percent;
}

static void fc_redraw(struct FileCopyJob* job) {
    job->drawn_percent = fc_percent(job);
    dwm_window_send_event(job->window, DWM_EVENT_REDRAW);
}

// New file or phase: always repaint
static void fc_set_stage(struct FileCopyJob* job, FileCopyPhase phase, const char* path, uint32_t size) {
    job->phase = phase;
    job->done  = 0;
    job->size  = size;
    if (path != NULL) {
        strncpy(job->name, fc_basename(path), sizeof(job->name) - 1);
        job->name[sizeof(job->name) - 1] = '\0';
    }
    fc_redraw(job);
}

// Progress within a file: repaint only when the percent moves
static void fc_set_done(struct FileCopyJob* job, uint32_t done) {
    job->done = done;
    if (fc_percent(job) != job->drawn_percent) fc_redraw(job);
}

// ==========================================
// Copying (copy thread, big lock held on entry and exit)
// ==========================================

static void fc_unlock(void) { mutex_unlock(fc_big_lock); }
static void fc_lock(void)   { mutex_lock(fc_big_lock); }

// vfs_read / vfs_write may move fewer bytes than asked; loop until done
static bool fc_read_full(File file, uint8_t* buffer, uint32_t count) {
    uint32_t got = 0;
    while (got < count) {
        int32_t n = vfs_read(file, buffer + got, count - got);
        if (n <= 0) return false;
        got += (uint32_t)n;
    }
    return true;
}

static bool fc_write_full(File file, const uint8_t* buffer, uint32_t count) {
    uint32_t put = 0;
    while (put < count) {
        int32_t n = vfs_write(file, buffer + put, count - put);
        if (n <= 0) return false;
        put += (uint32_t)n;
    }
    return true;
}

// Every chunk seeks to its own offset instead of trusting the file position
// left by the last call: other threads use the VFS between our chunks.
static bool fc_seek(File file, uint32_t offset) {
    return vfs_seek(file, offset) == offset;
}

static bool fc_copy_file(struct FileCopyJob* job, const char* src, const char* dest) {
    File in = vfs_open(src, VFS_OPEN_READ);
    if (in == VFS_INVALID_FILE) {
        job->last_error = "an original could not be opened";
        return false;
    }
    uint32_t size = vfs_get_size(in);

    File out = vfs_open(dest, VFS_OPEN_CREATE | VFS_OPEN_READ | VFS_OPEN_WRITE);
    if (out == VFS_INVALID_FILE) {
        vfs_close(in);
        job->last_error = "a copy could not be created";
        return false;
    }
    vfs_close(out);
    out = VFS_INVALID_FILE;

    // Reserve the space first: writes don't reliably extend a file. This can
    // take a long time for a big file, so the desktop runs meanwhile.
    fc_set_stage(job, FC_PHASE_PREPARING, src, size);
    fc_unlock();
    bool ok = vfs_truncate(dest, size);
    fc_lock();

    // vfs_truncate reports success even when the file system could not
    // resize (do_truncate ignores fs_file_resize's result), so check the
    // size the file really has. Writing past it would silently lose data.
    if (ok) {
        FSFileStats stats;
        ok = vfs_stat(dest, &stats) && stats.size == size;
    }
    if (!ok) {
        job->last_error = "there was not enough space for a copy";
    }
    if (ok && job->cancel) ok = false;

    if (ok && size > 0) {
        out = vfs_open(dest, VFS_OPEN_READ | VFS_OPEN_WRITE);
        if (out == VFS_INVALID_FILE) {
            ok = false;
            job->last_error = "a copy could not be opened for writing";
        }
    }

    // ---- Copy ----
    if (ok && size > 0) {
        fc_set_stage(job, FC_PHASE_COPYING, NULL, size);

        for (uint32_t offset = 0; offset < size; ) {
            uint32_t n = size - offset;
            if (n > FILECOPY_CHUNK) n = FILECOPY_CHUNK;

            fc_unlock();
            ok = fc_seek(in, offset)  && fc_read_full(in, job->buffer, n) &&
                 fc_seek(out, offset) && fc_write_full(out, job->buffer, n);
            fc_lock();

            if (!ok) {
                job->last_error = "reading or writing failed";
                break;
            }

            offset += n;
            fc_set_done(job, offset);

            if (job->cancel) {
                ok = false;
                break;
            }
        }
    }

#if FILECOPY_VERIFY
    // ---- Verify: read the copy back and compare ----
    if (ok && size > 0) {
        fc_set_stage(job, FC_PHASE_VERIFYING, NULL, size);

        for (uint32_t offset = 0; offset < size; ) {
            uint32_t n = size - offset;
            if (n > FILECOPY_CHUNK) n = FILECOPY_CHUNK;

            fc_unlock();
            bool same = fc_seek(in, offset)  && fc_read_full(in, job->buffer, n) &&
                        fc_seek(out, offset) && fc_read_full(out, job->verify_buffer, n);
            for (uint32_t i = 0; same && i < n; i++) {
                if (job->buffer[i] != job->verify_buffer[i]) same = false;
            }
            fc_lock();

            if (!same) {
                ok = false;
                job->last_error = "a copy did not match its original";
                break;
            }

            offset += n;
            fc_set_done(job, offset);

            if (job->cancel) {
                ok = false;
                break;
            }
        }
    }
#endif

    if (out != VFS_INVALID_FILE) vfs_close(out);
    vfs_close(in);

    // Empty files show as done too
    if (ok && size == 0) fc_set_stage(job, FC_PHASE_COPYING, NULL, 0);

    // Same permissions as the original
    if (ok) {
        uint8_t perm = 0;
        if (vfs_get_permissions(src, &perm))
            vfs_set_permissions(dest, perm);
    }

    return ok;
}

static bool fc_copy_any(struct FileCopyJob* job, const char* src, const char* dest, uint32_t depth);

static bool fc_copy_directory(struct FileCopyJob* job, const char* src, const char* dest, uint32_t depth) {
    if (depth >= FC_MAX_DEPTH) {
        job->last_error = "folders are nested too deeply";
        return false;
    }

    if (!vfs_mkdir(dest) || !vfs_directory_check(dest)) {
        job->last_error = "a folder could not be created";
        return false;
    }

    // On the heap: the copy thread's stack is 16 KB and this recurses
    char* child_src  = (char*)malloc(FILECOPY_PATH_MAX);
    char* child_dest = (char*)malloc(FILECOPY_PATH_MAX);
    if (child_src == NULL || child_dest == NULL) {
        if (child_src)  free(child_src);
        if (child_dest) free(child_dest);
        job->last_error = "out of memory";
        return false;
    }

    bool ok = true;
    uint32_t count = vfs_directory_get_item_count(src);

    for (uint32_t i = 0; i < count && ok; i++) {
        char name[32];
        memset(name, '\0', sizeof(name));
        if (!vfs_directory_get_item(src, i, name)) continue;
        if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        if (!fc_join(child_src,  FILECOPY_PATH_MAX, src,  name) ||
            !fc_join(child_dest, FILECOPY_PATH_MAX, dest, name)) {
            job->last_error = "a path was too long";
            ok = false;
            break;
        }

        ok = fc_copy_any(job, child_src, child_dest, depth + 1);
    }

    free(child_src);
    free(child_dest);

    if (ok) {
        uint8_t perm = 0;
        if (vfs_get_permissions(src, &perm))
            vfs_set_permissions(dest, perm);
    }

    return ok;
}

static bool fc_copy_any(struct FileCopyJob* job, const char* src, const char* dest, uint32_t depth) {
    if (job->cancel) return false;

    // Never descend into another device mounted somewhere inside the tree
    if (vfs_directory_check_mounted(src)) {
        job->last_error = "storage devices cannot be copied";
        return false;
    }

    if (vfs_directory_check(src))
        return fc_copy_directory(job, src, dest, depth);
    return fc_copy_file(job, src, dest);
}

// One top-level item. Returns false when the job should stop (cancelled).
static bool fc_run_item(struct FileCopyJob* job, struct FileCopyItem* item, const char* desktop_dir) {
    char dest[FILECOPY_PATH_MAX];
    char dest_dir[FILECOPY_PATH_MAX];
    char dest_name[VFS_NAME_MAX + 1];
    bool move = (job->flags & FILECOPY_MOVE) != 0;

    fc_set_stage(job, FC_PHASE_PREPARING, item->src, 0);

    if (!vfs_exists(item->src)) {
        job->last_error = "an original no longer exists";
        job->result.failed++;
        return true;
    }

    // Moving something onto itself: nothing to do
    if (move && strcmp(item->src, item->dest) == 0) {
        job->result.skipped++;
        return true;
    }


    // Settle the final destination name
    fc_parent(item->dest, dest_dir, sizeof(dest_dir));
    strncpy(dest, item->dest, sizeof(dest) - 1);
    dest[sizeof(dest) - 1] = '\0';
    strncpy(dest_name, fc_basename(item->dest), sizeof(dest_name) - 1);
    dest_name[sizeof(dest_name) - 1] = '\0';

    if (vfs_exists(dest)) {
        if (!(job->flags & FILECOPY_RENAME) ||
            !dwm_path_unique(dest_dir, fc_basename(item->dest), dest, sizeof(dest), dest_name)) {
            job->last_error = "the destination name is taken";
            job->result.failed++;
            return true;
        }
    }

    // Checked on the final name: pasting a folder next to itself becomes
    // "dir (2)", which is fine, but "dir/inner" never is
    if (fc_path_is_within(dest, item->src)) {
        job->last_error = "a folder cannot be copied into itself";
        job->result.failed++;
        return true;
    }

    // Nothing exists at `dest`, so anything there after a failure is ours
    if (!fc_copy_any(job, item->src, dest, 0)) {
        if (vfs_exists(dest)) vfs_remove(dest);

        if (job->cancel) {
            job->result.cancelled = true;
            return false;
        }
        job->result.failed++;
        return true;
    }

    job->result.copied++;

    if (desktop_dir != NULL && strcmp(dest_dir, desktop_dir) == 0) {
        if (!job->used_origin && job->desktop_x >= 0 && job->desktop_y >= 0) {
            job->used_origin = true;
            dwm_desktop_add_item_icon(dest, dest_name, job->desktop_x, job->desktop_y);
        } else {
            dwm_desktop_add_item_icon(dest, dest_name, -1, -1);
        }
    }

    if (move) {
        // A move is a copy plus a delete. If the delete fails the item
        // exists twice, which is safer than losing it.
        if (vfs_remove(item->src)) {
            dwm_desktop_forget_path(item->src);
        } else {
            job->last_error = "an original could not be removed after copying";
            job->result.failed++;
        }
    }

    // Let open explorer windows show each item as it lands
    kernel_event_send(KEVENT_DWM_REFRESH, "", "");
    return true;
}

static void fc_report(struct FileCopyJob* job) {
    if (job->result.failed == 0 || job->last_error == NULL) return;

    char message[160];
    size_t n = fc_u32_to_str(job->result.failed, message);
    message[n] = '\0';
    strncat(message, (job->result.failed == 1) ? " item" : " items", sizeof(message) - strlen(message) - 1);
    strncat(message, " could not be ", sizeof(message) - strlen(message) - 1);
    strncat(message, (job->flags & FILECOPY_MOVE) ? "moved: " : "copied: ", sizeof(message) - strlen(message) - 1);
    strncat(message, job->last_error, sizeof(message) - strlen(message) - 1);

    dwm_summon_message_box((job->flags & FILECOPY_MOVE) ? "Move" : "Copy", message);
}

static void fc_free_job(struct FileCopyJob* job) {
    struct FileCopyItem* item = job->items_head;
    while (item != NULL) {
        struct FileCopyItem* next = item->next;
        free(item);
        item = next;
    }
    if (job->buffer != NULL)        free(job->buffer);
    if (job->verify_buffer != NULL) free(job->verify_buffer);
    free(job);
}

static void fc_run_job(struct FileCopyJob* job) {
    char desktop_dir[FILECOPY_PATH_MAX];
    const char* desktop = dwm_desktop_get_directory(desktop_dir, sizeof(desktop_dir)) ? desktop_dir : NULL;

    job->buffer        = (uint8_t*)malloc(FILECOPY_CHUNK);
    job->verify_buffer = (uint8_t*)malloc(FILECOPY_CHUNK);
    if (job->buffer == NULL || job->verify_buffer == NULL) {
        job->last_error = "out of memory";
        job->result.failed = job->item_count;
        return;
    }

    // Items may still be added while this runs (re-read `next` each time)
    for (struct FileCopyItem* item = job->items_head; item != NULL; item = item->next) {
        if (job->cancel) {
            job->result.cancelled = true;
            break;
        }
        job->item_index++;
        if (!fc_run_item(job, item, desktop)) break;
    }
}

static void fc_thread_main(void) {
    while (1) {
        event_wait(&fc_event, 0);           // 0 = until signaled

        fc_lock();

        while (fc_jobs != NULL) {
            struct FileCopyJob* job = fc_jobs;

            fc_run_job(job);

            if (job->done_callback != NULL)
                job->done_callback(&job->result, job->done_user);
            fc_report(job);

            // Off the list first: from here on the window's handler finds
            // nothing and ignores whatever events are still queued for it
            fc_jobs = job->next;
            dwm_window_send_event(job->window, DWM_EVENT_CLOSE);
            fc_free_job(job);

            // The next window stops saying "Waiting"
            if (fc_jobs != NULL) fc_redraw(fc_jobs);
        }

        fc_unlock();
    }
}

// ==========================================
// Drawing
// ==========================================

static void fc_draw(struct WindowObject* window, struct FileCopyJob* job) {
    bool move = (job->flags & FILECOPY_MOVE) != 0;

    dwm_draw_rect_filled(0, 0, window->w, window->h, theme.client.background);

    // Status line
    char status[48];
    status[0] = '\0';
    switch (job->phase) {
    case FC_PHASE_WAITING:   strncpy(status, "Waiting for another copy", sizeof(status) - 1); break;
    case FC_PHASE_PREPARING: strncpy(status, "Preparing", sizeof(status) - 1); break;
    case FC_PHASE_COPYING:   strncpy(status, move ? "Moving" : "Copying", sizeof(status) - 1); break;
    case FC_PHASE_VERIFYING: strncpy(status, "Verifying", sizeof(status) - 1); break;
    }
    status[sizeof(status) - 1] = '\0';

    if (job->phase != FC_PHASE_WAITING && job->item_count > 1 && job->item_index > 0) {
        char num[12];
        strncat(status, " ", sizeof(status) - strlen(status) - 1);
        fc_u32_to_str(job->item_index, num);
        strncat(status, num, sizeof(status) - strlen(status) - 1);
        strncat(status, " of ", sizeof(status) - strlen(status) - 1);
        fc_u32_to_str(job->item_count, num);
        strncat(status, num, sizeof(status) - strlen(status) - 1);
    }
    dwm_draw_text(FC_PAD, FC_STATUS_Y, status, theme.client.text);
    dwm_draw_text(FC_PAD, FC_NAME_Y, job->name, theme.client.text_value);

    // Bar
    int bar_w   = (int)window->w - (FC_PAD * 2);
    int percent = fc_percent(job);

    dwm_draw_rect_filled(FC_PAD, FC_BAR_Y, bar_w, FC_BAR_H, theme.edit.background);
    if (percent > 0) {
        dwm_draw_rect_filled(FC_PAD, FC_BAR_Y, (bar_w * percent) / 100, FC_BAR_H, theme.client.accent);
    }
    dwm_draw_rect(FC_PAD - 1, FC_BAR_Y - 1, bar_w + 2, FC_BAR_H + 2, theme.edit.border);

    // Percent, or the cancel acknowledgement
    if (job->cancel) {
        dwm_draw_text(FC_PAD, FC_PERCENT_Y, "Cancelling...", theme.client.text_muted);
    } else if (job->phase == FC_PHASE_COPYING || job->phase == FC_PHASE_VERIFYING) {
        char pct[8];
        size_t n = fc_u32_to_str((uint32_t)percent, pct);
        pct[n]     = '%';
        pct[n + 1] = '\0';
        dwm_draw_text(FC_PAD, FC_PERCENT_Y, pct, theme.client.text_muted);
    }

    // Cancel button (bottom right of the client area)
    int client_h = (int)window->h - (int)window->titlebar_height;
    int btn_x = (int)window->w - FC_PAD - FC_BUTTON_W;
    int btn_y = client_h - FC_PAD - FC_BUTTON_H + 4;

    const char* label = "Cancel";
    int label_w = (int)strlen(label) * FC_FONT_W;

    dwm_draw_rect_filled(btn_x, btn_y, FC_BUTTON_W, FC_BUTTON_H, theme.button.fill);
    dwm_draw_rect(btn_x, btn_y, FC_BUTTON_W, FC_BUTTON_H, theme.button.border);
    dwm_draw_text(btn_x + (FC_BUTTON_W - label_w) / 2, btn_y + (FC_BUTTON_H - FC_FONT_H) / 2,
                  label, job->cancel ? theme.client.text_muted : theme.button.text);
}

// ==========================================
// Event handler (DWM thread)
// ==========================================

static void fc_request_cancel(WindowHandle handle, struct FileCopyJob* job) {
    if (job->cancel) return;
    job->cancel = true;
    dwm_window_send_event(handle, DWM_EVENT_REDRAW);
}

void callback_filecopy_handler(WindowHandle handle, wEvent event, uint32_t wparam, int32_t lparam) {
    struct WindowObject* window = dwm_get_window_by_id(handle);
    struct FileCopyJob* job = fc_find(handle);
    if (window == NULL || job == NULL) return;

    switch (event) {
        case DWM_EVENT_REDRAW:
            fc_draw(window, job);
            break;

        case DWM_EVENT_MOUSE: {
            if (!(lparam & DWM_STATE_MOUSE_BTN_LEFT)) break;

            int click_x = (int16_t)(wparam & 0xFFFF);
            int click_y = (int16_t)((wparam >> 16) & 0xFFFF);

            int client_h = (int)window->h - (int)window->titlebar_height;
            int btn_x = (int)window->w - FC_PAD - FC_BUTTON_W;
            int btn_y = client_h - FC_PAD - FC_BUTTON_H + 4;

            if (click_x >= btn_x && click_x < btn_x + FC_BUTTON_W &&
                click_y >= btn_y && click_y < btn_y + FC_BUTTON_H) {
                fc_request_cancel(handle, job);
            }
            break;
        }

        case DWM_EVENT_KEYBOARD:
            if ((wparam & 0xFF) == FC_KEY_ESCAPE) fc_request_cancel(handle, job);
            break;

        default:
            break;
    }
}

// ==========================================
// Public API
// ==========================================

void dwm_filecopy_init(mutex_t* big_lock) {
    fc_big_lock = big_lock;
}

static bool fc_add_item(struct FileCopyJob* job, const char* src, const char* dest) {
    if (src == NULL || dest == NULL || src[0] == '\0' || dest[0] == '\0') return false;
    if (strnlen(src,  FILECOPY_PATH_MAX) >= FILECOPY_PATH_MAX) return false;
    if (strnlen(dest, FILECOPY_PATH_MAX) >= FILECOPY_PATH_MAX) return false;

    struct FileCopyItem* item = (struct FileCopyItem*)malloc(sizeof(struct FileCopyItem));
    if (item == NULL) return false;

    memset(item, 0, sizeof(struct FileCopyItem));
    strncpy(item->src,  src,  FILECOPY_PATH_MAX - 1);
    strncpy(item->dest, dest, FILECOPY_PATH_MAX - 1);

    if (job->items_tail != NULL) job->items_tail->next = item;
    else                         job->items_head = item;
    job->items_tail = item;
    job->item_count++;
    return true;
}

WindowHandle dwm_summon_filecopy(const char* src, const char* dest, uint32_t flags) {
    if (fc_big_lock == NULL) return 0;

    struct FileCopyJob* job = (struct FileCopyJob*)malloc(sizeof(struct FileCopyJob));
    if (job == NULL) return 0;

    memset(job, 0, sizeof(struct FileCopyJob));
    job->flags         = flags;
    job->desktop_x     = -1;
    job->desktop_y     = -1;
    job->drawn_percent = -1;
    job->phase         = (fc_jobs != NULL) ? FC_PHASE_WAITING : FC_PHASE_PREPARING;

    if (!fc_add_item(job, src, dest)) {
        fc_free_job(job);
        return 0;
    }
    strncpy(job->name, fc_basename(src), sizeof(job->name) - 1);

    // The copy thread is started the first time it is needed
    if (fc_thread == NULL) {
        fc_thread = thread_create(fc_thread_main, PRIORITY_NORMAL);
        if (fc_thread == NULL) {
            fc_free_job(job);
            return 0;
        }
    }

    WindowClass wclass;
    memset(&wclass, 0, sizeof(WindowClass));
    wclass.width      = FC_WIDTH;
    wclass.height     = FC_HEIGHT;
    wclass.max_width  = FC_WIDTH;
    wclass.max_height = FC_HEIGHT;

    // Centered above the taskbar, nudged down for each window already open
    uint32_t open_jobs = 0;
    for (struct FileCopyJob* j = fc_jobs; j != NULL; j = j->next) open_jobs++;
    wclass.x = (uint16_t)((display_get_width() - FC_WIDTH) / 2 + (open_jobs % 5) * 20);
    wclass.y = (uint16_t)((display_get_height() - taskbar.height - FC_HEIGHT) / 2 + (open_jobs % 5) * 20);

    strncpy(wclass.title, (flags & FILECOPY_MOVE) ? "Moving files" : "Copying files", DWM_MAX_TITLE_LEN - 1);

    // Joins the list before the window exists, so its first REDRAW finds it
    struct FileCopyJob** tail = &fc_jobs;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = job;

    // No close box: Cancel is the way out, so a job always ends cleanly
    WindowHandle window = dwm_create_window(wclass, DWM_WSTYLE_NOCLOSEBOX, callback_filecopy_handler);
    if (window == 0) {
        *tail = NULL;
        fc_free_job(job);
        return 0;
    }
    job->window = window;

    // The thread can't start before the caller releases kernel_big_lock, so
    // items added right after this call are picked up
    event_signal(&fc_event);
    return window;
}

bool dwm_filecopy_add(WindowHandle handle, const char* src, const char* dest) {
    struct FileCopyJob* job = fc_find(handle);
    if (job == NULL) return false;
    if (!fc_add_item(job, src, dest)) return false;
    fc_redraw(job);
    return true;
}

void dwm_filecopy_set_desktop_origin(WindowHandle handle, int x, int y) {
    struct FileCopyJob* job = fc_find(handle);
    if (job == NULL) return;
    job->desktop_x = x;
    job->desktop_y = y;
}

void dwm_filecopy_on_done(WindowHandle handle, FileCopyDoneCallback callback, void* user) {
    struct FileCopyJob* job = fc_find(handle);
    if (job == NULL) return;
    job->done_callback = callback;
    job->done_user     = user;
}

bool dwm_filecopy_busy(void) {
    return fc_jobs != NULL;
}
