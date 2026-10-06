#ifndef _FILECOPY_WINDOW_H_
#define _FILECOPY_WINDOW_H_

#include <kernel/dwm/dwm.h>
#include <kernel/mutex.h>

// File copy / move window.
//
//   +-- Copying files ------------------------+
//   | Copying 2 of 5                          |
//   | holiday.bmp                             |
//   | [##############                       ] |
//   | 37%                          [ Cancel ] |
//   +-----------------------------------------+
//
// Give it a source and a destination path and it does the rest: the window
// opens right away, a background thread (owned by this module, started on
// first use) copies the file or folder tree, and the window closes itself
// when the job is done. Several windows can be open; their jobs run one at a
// time, in order, and a waiting window says so.
//
// Responsiveness: the thread only holds kernel_big_lock for bookkeeping
// (heap, opening files, updating the window). Reading, writing and
// reserving space on disk happen without it, so the desktop keeps drawing
// and taking input even during one long disk operation.
//
// Correctness: after reserving space the copier checks the file really got
// that size, and (FILECOPY_VERIFY) reads the copy back and compares it with
// the original. A copy that fails either check is deleted and reported,
// never left behind looking complete.
//
// Threading: call these from thread context with kernel_big_lock held (any
// window procedure or DWM code).

// Read the copy back and compare it with the original (doubles the reading)
#ifndef FILECOPY_VERIFY
#define FILECOPY_VERIFY         1
#endif

// Bytes per read / write
#ifndef FILECOPY_CHUNK
#define FILECOPY_CHUNK          4096
#endif

// Longest source / destination path
#define FILECOPY_PATH_MAX       256

// Flags for dwm_summon_filecopy
#define FILECOPY_MOVE           0x01    // Delete each original once its copy succeeded
#define FILECOPY_RENAME         0x02    // Destination taken: use "name (2)" etc. instead of failing

typedef struct {
    uint32_t copied;        // Items that arrived at their destination
    uint32_t failed;        // Items that did not (nothing is left behind for these)
    uint32_t skipped;       // Moves whose source and destination were the same
    bool     cancelled;     // The user pressed Cancel
} FileCopyResult;

// Called on the copy thread, kernel_big_lock held, when the job ends
// (right before the window closes)
typedef void (*FileCopyDoneCallback)(const FileCopyResult* result, void* user);

// Wire up once in kmain, before the DWM starts: the lock every heap and DWM
// user holds. Until this is called dwm_summon_filecopy returns 0.
void dwm_filecopy_init(mutex_t* big_lock);

// Open a copy window for one item: `src` is a file or folder, `dest` the
// full path it should end up at (a folder is copied with everything in it).
// Returns 0 if the window or the job could not be created.
WindowHandle dwm_summon_filecopy(const char* src, const char* dest, uint32_t flags);

// Add another item to the same window (e.g. several files pasted at once).
// Works until the job has finished. Returns false if the window is gone.
bool dwm_filecopy_add(WindowHandle handle, const char* src, const char* dest);

// Where the first desktop icon goes when items land in the desktop folder
// (the default is the next free spot on the desktop grid)
void dwm_filecopy_set_desktop_origin(WindowHandle handle, int x, int y);

// Get told how the job went (e.g. to finish a clipboard cut)
void dwm_filecopy_on_done(WindowHandle handle, FileCopyDoneCallback callback, void* user);

// True while any copy window has work queued or running (e.g. hold off
// shutting down)
bool dwm_filecopy_busy(void);

#endif
