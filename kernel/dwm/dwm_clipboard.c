
#include <kernel/dwm/dwm_clipboard.h>
#include <kernel/dwm/dwm.h>
#include <kernel/dwm/dwm_core_internal.h>

#include <kernel/kernel.h>
#include <kernel/events.h>
#include <kernel/mutex.h>
#include <kernel/memory/malloc.h>
#include <kernel/vfs/vfs.h>
#include <kernel/console/display.h>
#include <kernel/util/list.h>
#include <kernel/util/string.h>

// Storage per format:
//
//   TEXT     data = text + '\0', size = text length (without the '\0')
//   FILES    data = "path0\0path1\0...pathN\0", size = total bytes
//   IMAGE    data = width * height ARGB pixels, size = width * height * 4
//   BINARY   data = the bytes, size = byte count
//
// New contents are always built in a private buffer first and swapped in
// under the lock, so a failed set (bad input, out of memory) leaves the old
// contents untouched and the lock is never held across a copy of caller data.
//
// The mutex needs no init call: a zeroed mutex_t is a valid unlocked mutex
// (see mutex_init), so the clipboard works even before dwm_initiate.
// Lock order is kernel_big_lock -> clip_lock; nothing here takes the big lock.

static mutex_t clip_lock;

static struct {
    DWMClipboardFormat format;
    uint8_t*           data;
    uint32_t           size;
    uint32_t           file_count;
    DWMClipboardFileOp file_op;
    uint16_t           image_width;
    uint16_t           image_height;
    uint32_t           sequence;
} clip;

//
// Internal helpers
//

// Detach the payload and mark the clipboard empty. Lock must be held.
// Returns the old buffer for the caller to free after unlocking.
static uint8_t* clip_take_locked(void) {
    uint8_t* old = clip.data;
    
    clip.format       = DWM_CLIPBOARD_EMPTY;
    clip.data         = NULL;
    clip.size         = 0;
    clip.file_count   = 0;
    clip.file_op      = DWM_CLIPBOARD_OP_COPY;
    clip.image_width  = 0;
    clip.image_height = 0;
    
    return old;
}

// Swap in a fully built payload (ownership passes to the clipboard)
static void clip_install(DWMClipboardFormat format, uint8_t* data, uint32_t size,
                         uint32_t file_count, DWMClipboardFileOp op,
                         uint16_t width, uint16_t height) {
    mutex_lock(&clip_lock);
    
    uint8_t* old = clip_take_locked();
    
    clip.format       = format;
    clip.data         = data;
    clip.size         = size;
    clip.file_count   = file_count;
    clip.file_op      = op;
    clip.image_width  = width;
    clip.image_height = height;
    clip.sequence++;
    
    mutex_unlock(&clip_lock);
    
    if (old != NULL) free(old);
}

// BINARY counts as text when it has no '\0' (old callers stored text this way)
static bool clip_binary_is_text_locked(void) {
    if (clip.format != DWM_CLIPBOARD_BINARY || clip.size == 0) return false;
    for (uint32_t i = 0; i < clip.size; i++) {
        if (clip.data[i] == 0) return false;
    }
    return true;
}

// Copy up to size - 1 bytes and terminate. FILES store their paths separated
// by '\0'; with nul_to_newline those become '\n' (the final '\0' is never
// copied because len excludes it).
static void clip_copy_text_out(char* out, uint32_t size, const uint8_t* src,
                               uint32_t len, bool nul_to_newline) {
    if (out == NULL || size == 0) return;
    
    uint32_t n = (len < size - 1) ? len : size - 1;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = src[i];
        out[i] = (char)((c == 0 && nul_to_newline) ? '\n' : c);
    }
    out[n] = '\0';
}

//
// General
//

void dwm_clipboard_clear(void) {
    mutex_lock(&clip_lock);
    
    if (clip.format == DWM_CLIPBOARD_EMPTY) {
        mutex_unlock(&clip_lock);
        return;
    }
    
    uint8_t* old = clip_take_locked();
    clip.sequence++;
    
    mutex_unlock(&clip_lock);
    
    if (old != NULL) free(old);
}

DWMClipboardFormat dwm_clipboard_get_format(void) {
    mutex_lock(&clip_lock);
    DWMClipboardFormat format = clip.format;
    mutex_unlock(&clip_lock);
    return format;
}

bool dwm_clipboard_has_format(DWMClipboardFormat format) {
    bool result;
    
    mutex_lock(&clip_lock);
    
    if (format == DWM_CLIPBOARD_TEXT) {
        result = (clip.format == DWM_CLIPBOARD_TEXT) ||
                 (clip.format == DWM_CLIPBOARD_FILES) ||
                 clip_binary_is_text_locked();
    } else {
        result = (clip.format == format);
    }
    
    mutex_unlock(&clip_lock);
    return result;
}

uint32_t dwm_clipboard_get_sequence(void) {
    mutex_lock(&clip_lock);
    uint32_t sequence = clip.sequence;
    mutex_unlock(&clip_lock);
    return sequence;
}

void dwm_clipboard_get_info(DWMClipboardInfo* out) {
    if (out == NULL) return;
    
    mutex_lock(&clip_lock);
    out->format       = clip.format;
    out->size         = clip.size;
    out->file_count   = clip.file_count;
    out->file_op      = clip.file_op;
    out->image_width  = clip.image_width;
    out->image_height = clip.image_height;
    out->sequence     = clip.sequence;
    mutex_unlock(&clip_lock);
}

//
// Text
//

bool dwm_clipboard_set_text_n(const char* text, uint32_t length) {
    if (text == NULL) return false;
    
    length = (uint32_t)strnlen(text, length);
    if (length == 0) {
        dwm_clipboard_clear();
        return true;
    }
    if (length > DWM_CLIPBOARD_MAX_BYTES - 1) return false;
    
    uint8_t* copy = (uint8_t*)malloc(length + 1);
    if (copy == NULL) return false;
    
    memcpy(copy, text, length);
    copy[length] = '\0';
    
    clip_install(DWM_CLIPBOARD_TEXT, copy, length, 0, DWM_CLIPBOARD_OP_COPY, 0, 0);
    return true;
}

bool dwm_clipboard_set_text(const char* text) {
    return dwm_clipboard_set_text_n(text, DWM_CLIPBOARD_MAX_BYTES);
}

uint32_t dwm_clipboard_get_text(char* out, uint32_t size) {
    if (out != NULL && size > 0) out[0] = '\0';
    
    uint32_t length = 0;
    
    mutex_lock(&clip_lock);
    
    switch (clip.format) {
    case DWM_CLIPBOARD_TEXT:
        length = clip.size;
        clip_copy_text_out(out, size, clip.data, length, false);
        break;
    
    case DWM_CLIPBOARD_FILES:
        // Paths joined by '\n': the packed list minus its final '\0'
        length = clip.size - 1;
        clip_copy_text_out(out, size, clip.data, length, true);
        break;
    
    case DWM_CLIPBOARD_BINARY:
        if (clip_binary_is_text_locked()) {
            length = clip.size;
            clip_copy_text_out(out, size, clip.data, length, false);
        }
        break;
    
    default:
        break;
    }
    
    mutex_unlock(&clip_lock);
    return length;
}

//
// File paths
//

bool dwm_clipboard_set_files(const char* const* paths, uint32_t count, DWMClipboardFileOp op) {
    if (paths == NULL || count == 0 || count > DWM_CLIPBOARD_MAX_FILES) return false;
    if (op != DWM_CLIPBOARD_OP_COPY && op != DWM_CLIPBOARD_OP_CUT) return false;
    
    // Validate everything before touching the clipboard
    uint32_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (paths[i] == NULL) return false;
        
        size_t len = strnlen(paths[i], DWM_MAX_PATH_LEN);
        if (len == 0 || len >= DWM_MAX_PATH_LEN) return false;
        
        total += (uint32_t)len + 1;
    }
    
    uint8_t* list = (uint8_t*)malloc(total);
    if (list == NULL) return false;
    
    uint32_t offset = 0;
    for (uint32_t i = 0; i < count; i++) {
        size_t len = strnlen(paths[i], DWM_MAX_PATH_LEN);
        memcpy(&list[offset], paths[i], len);
        offset += (uint32_t)len;
        list[offset++] = '\0';
    }
    
    clip_install(DWM_CLIPBOARD_FILES, list, total, count, op, 0, 0);
    return true;
}

bool dwm_clipboard_set_file(const char* path, DWMClipboardFileOp op) {
    return dwm_clipboard_set_files(&path, 1, op);
}

uint32_t dwm_clipboard_get_file_count(void) {
    mutex_lock(&clip_lock);
    uint32_t count = (clip.format == DWM_CLIPBOARD_FILES) ? clip.file_count : 0;
    mutex_unlock(&clip_lock);
    return count;
}

bool dwm_clipboard_get_file(uint32_t index, char* out, uint32_t size) {
    if (out == NULL || size == 0) return false;
    out[0] = '\0';
    
    bool ok = false;
    
    mutex_lock(&clip_lock);
    
    if (clip.format == DWM_CLIPBOARD_FILES && index < clip.file_count) {
        const char* path = (const char*)clip.data;
        for (uint32_t i = 0; i < index; i++)
            path += strlen(path) + 1;
        
        size_t len = strlen(path);
        if (len < size) {
            memcpy(out, path, len + 1);
            ok = true;
        }
    }
    
    mutex_unlock(&clip_lock);
    return ok;
}

bool dwm_clipboard_files_complete_cut(uint32_t sequence) {
    mutex_lock(&clip_lock);
    
    if (clip.sequence != sequence ||
        clip.format != DWM_CLIPBOARD_FILES ||
        clip.file_op != DWM_CLIPBOARD_OP_CUT) {
        mutex_unlock(&clip_lock);
        return false;
    }
    
    // The cut paths no longer exist at their old location
    uint8_t* old = clip_take_locked();
    clip.sequence++;
    
    mutex_unlock(&clip_lock);
    
    if (old != NULL) free(old);
    return true;
}

//
// Images
//

bool dwm_clipboard_set_image(const uint32_t* pixels, uint16_t width, uint16_t height) {
    if (pixels == NULL || width == 0 || height == 0) return false;
    
    // 64-bit: 65535 * 65535 * 4 overflows 32 bits
    uint64_t bytes = (uint64_t)width * height * sizeof(uint32_t);
    if (bytes > DWM_CLIPBOARD_MAX_BYTES) return false;
    
    uint8_t* copy = (uint8_t*)malloc((uint32_t)bytes);
    if (copy == NULL) return false;
    memcpy(copy, pixels, (uint32_t)bytes);
    
    clip_install(DWM_CLIPBOARD_IMAGE, copy, (uint32_t)bytes, 0, DWM_CLIPBOARD_OP_COPY, width, height);
    return true;
}

bool dwm_clipboard_get_image(struct Image* out) {
    if (out == NULL) return false;
    out->data   = NULL;
    out->width  = 0;
    out->height = 0;
    
    bool ok = false;
    
    mutex_lock(&clip_lock);
    
    if (clip.format == DWM_CLIPBOARD_IMAGE) {
        uint32_t* pixels = (uint32_t*)malloc(clip.size);
        if (pixels != NULL) {
            memcpy(pixels, clip.data, clip.size);
            out->data   = pixels;
            out->width  = clip.image_width;
            out->height = clip.image_height;
            ok = true;
        }
    }
    
    mutex_unlock(&clip_lock);
    return ok;
}

//
// Raw bytes
//

bool dwm_clipboard_set(const void* data, uint32_t size) {
    if (data == NULL || size == 0) {
        dwm_clipboard_clear();
        return true;
    }
    if (size > DWM_CLIPBOARD_MAX_BYTES) return false;
    
    uint8_t* copy = (uint8_t*)malloc(size);
    if (copy == NULL) return false;   // Keep the old contents on failure
    memcpy(copy, data, size);
    
    clip_install(DWM_CLIPBOARD_BINARY, copy, size, 0, DWM_CLIPBOARD_OP_COPY, 0, 0);
    return true;
}

uint32_t dwm_clipboard_get_data(void* out, uint32_t size) {
    uint32_t total = 0;
    
    mutex_lock(&clip_lock);
    
    if (clip.format == DWM_CLIPBOARD_TEXT || clip.format == DWM_CLIPBOARD_BINARY) {
        total = clip.size;
        if (out != NULL && size > 0)
            memcpy(out, clip.data, (total < size) ? total : size);
    }
    
    mutex_unlock(&clip_lock);
    return total;
}

const uint8_t* dwm_clipboard_get(uint32_t* out_size) {
    const uint8_t* data = NULL;
    uint32_t size = 0;
    
    mutex_lock(&clip_lock);
    
    if (clip.format == DWM_CLIPBOARD_TEXT || clip.format == DWM_CLIPBOARD_BINARY) {
        data = clip.data;
        size = clip.size;
    }
    
    mutex_unlock(&clip_lock);
    
    if (out_size != NULL) *out_size = size;
    return data;
}

// ==========================================
// File commands (Cut / Copy / Paste menu items)
// ==========================================
//
// Pasting files hands the work to a copy window (dwm/windows/filecopy.c),
// which runs it on its own thread. These run on the DWM thread with
// kernel_big_lock held.

#define PASTE_TEXT_NAME      "clipping"   // File created when pasting text

static const char* paste_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}

static bool paste_join(char* out, size_t size, const char* dir, const char* name) {
    size_t dir_len  = strlen(dir);
    size_t name_len = strlen(name);
    bool   slash    = (dir_len == 0 || dir[dir_len - 1] != '/');
    
    if (dir_len + (slash ? 1 : 0) + name_len + 1 > size) return false;
    
    memcpy(out, dir, dir_len);
    if (slash) out[dir_len++] = '/';
    memcpy(&out[dir_len], name, name_len + 1);
    return true;
}

// True when `path` lies inside a mounted storage device (not the mount point
// itself and not one of the kernel's own /dev, /mnt, /proc nodes). Only
// those items can be copied: knode folders are not files on a disk.
static bool paste_path_is_on_device(const char* path) {
    char prefix[DWM_MAX_PATH_LEN];
    size_t len = strnlen(path, DWM_MAX_PATH_LEN);
    if (len == 0 || len >= DWM_MAX_PATH_LEN) return false;
    
    for (size_t i = 1; i < len; i++) {
        if (path[i] != '/') continue;
        memcpy(prefix, path, i);
        prefix[i] = '\0';
        if (vfs_directory_check_mounted(prefix)) return true;
    }
    return false;
}

// Text paste: write the clipboard text into a new "clipping" file
static bool paste_write_text(const char* dir, char* out_path, size_t path_size, char* out_name) {
    uint32_t length = dwm_clipboard_get_text(NULL, 0);
    if (length == 0) return false;
    
    if (!dwm_path_unique(dir, PASTE_TEXT_NAME, out_path, path_size, out_name))
        return false;
    
    char* text = (char*)malloc(length + 1);
    if (text == NULL) return false;
    
    uint32_t got = dwm_clipboard_get_text(text, length + 1);
    if (got > length) got = length;
    
    bool ok = false;
    
    File file = vfs_open(out_path, VFS_OPEN_CREATE | VFS_OPEN_READ | VFS_OPEN_WRITE);
    if (file != VFS_INVALID_FILE) {
        vfs_close(file);
        
        // Size first: writes don't reliably extend a file (see notepad_save)
        FSFileStats stats;
        if (vfs_truncate(out_path, got) && vfs_stat(out_path, &stats) && stats.size == got) {
            file = vfs_open(out_path, VFS_OPEN_READ | VFS_OPEN_WRITE);
            if (file != VFS_INVALID_FILE) {
                vfs_seek(file, 0);
                ok = (vfs_write(file, text, got) == (int32_t)got);
                vfs_close(file);
            }
        }
        
        if (!ok) vfs_remove(out_path);
    }
    
    free(text);
    return ok;
}

// A cut is finished once its copy window moved everything: the clipboard
// then empties (unless something else was copied in the meantime)
static void paste_cut_done(const FileCopyResult* result, void* user) {
    uint32_t sequence = (uint32_t)(uintptr_t)user;
    if (!result->cancelled && result->copied > 0 && result->failed == 0)
        dwm_clipboard_files_complete_cut(sequence);
}

bool dwm_clipboard_put_file(const char* path, DWMClipboardFileOp op) {
    const char* title = (op == DWM_CLIPBOARD_OP_CUT) ? "Cut" : "Copy";
    
    if (path == NULL || !vfs_exists(path)) {
        dwm_summon_message_box(title, "That item no longer exists");
        return false;
    }
    if (strnlen(path, DWM_MAX_PATH_LEN) >= DWM_MAX_PATH_LEN) {
        dwm_summon_message_box(title, "The path is too long for the clipboard");
        return false;
    }
    if (vfs_directory_check_mounted(path)) {
        dwm_summon_message_box(title, "Storage devices cannot be copied or moved");
        return false;
    }
    if (!paste_path_is_on_device(path)) {
        dwm_summon_message_box(title, "System folders cannot be copied or moved");
        return false;
    }
    if (!dwm_clipboard_set_file(path, op)) {
        dwm_summon_message_box(title, "Not enough memory for the clipboard");
        return false;
    }
    return true;
}

bool dwm_clipboard_paste_into(const char* dest_dir, int icon_x, int icon_y) {
    // Normalized destination (no trailing slash except for "/")
    char dir[DWM_MAX_PATH_LEN];
    if (dest_dir == NULL || dest_dir[0] == '\0' ||
        strnlen(dest_dir, DWM_MAX_PATH_LEN) >= DWM_MAX_PATH_LEN) {
        dwm_summon_message_box("Paste", "Cannot paste into this location");
        return false;
    }
    memset(dir, '\0', sizeof(dir));
    strncpy(dir, dest_dir, sizeof(dir) - 1);
    size_t dir_len = strlen(dir);
    while (dir_len > 1 && dir[dir_len - 1] == '/') dir[--dir_len] = '\0';
    
    if (!vfs_directory_check(dir) || (!vfs_directory_check_mounted(dir) && !paste_path_is_on_device(dir))) {
        dwm_summon_message_box("Paste", "Cannot paste into this location");
        return false;
    }
    
    DWMClipboardInfo info;
    dwm_clipboard_get_info(&info);
    
    // ---- Files: one copy window for everything on the clipboard ----
    if (info.format == DWM_CLIPBOARD_FILES) {
        bool cut = (info.file_op == DWM_CLIPBOARD_OP_CUT);
        uint32_t flags = FILECOPY_RENAME | (cut ? FILECOPY_MOVE : 0);
        
        char src[DWM_MAX_PATH_LEN];
        char dest[FILECOPY_PATH_MAX];
        WindowHandle window = 0;
        
        for (uint32_t i = 0; i < info.file_count; i++) {
            if (!dwm_clipboard_get_file(i, src, sizeof(src))) continue;
            if (!paste_join(dest, sizeof(dest), dir, paste_basename(src))) continue;
            
            if (window == 0) {
                window = dwm_summon_filecopy(src, dest, flags);
                if (window == 0) {
                    dwm_summon_message_box("Paste", "Could not start copying");
                    return false;
                }
            } else {
                dwm_filecopy_add(window, src, dest);
            }
        }
        
        if (window == 0) return false;
        
        dwm_filecopy_set_desktop_origin(window, icon_x, icon_y);
        if (cut) dwm_filecopy_on_done(window, paste_cut_done, (void*)(uintptr_t)info.sequence);
        return true;
    }
    
    // ---- Text: written straight into a new file ----
    if (dwm_clipboard_has_format(DWM_CLIPBOARD_TEXT)) {
        char dest[DWM_MAX_PATH_LEN];
        char dest_name[VFS_NAME_MAX + 1];
        
        if (!paste_write_text(dir, dest, sizeof(dest), dest_name)) {
            dwm_summon_message_box("Paste", "The text could not be saved to a file here");
            return false;
        }
        
        char desktop_dir[DWM_MAX_PATH_LEN];
        if (dwm_desktop_get_directory(desktop_dir, sizeof(desktop_dir)) && strcmp(dir, desktop_dir) == 0)
            dwm_desktop_add_item_icon(dest, dest_name, icon_x, icon_y);
        
        kernel_event_send(KEVENT_DWM_REFRESH, "", "");
        return true;
    }
    
    dwm_summon_message_box("Paste", (info.format == DWM_CLIPBOARD_EMPTY)
                                    ? "The clipboard is empty"
                                    : "The clipboard holds nothing that can be pasted here");
    return false;
}
