#ifndef _DWM_CLIPBOARD_H_
#define _DWM_CLIPBOARD_H_

// System-wide clipboard.
//
// The clipboard holds one item at a time, tagged with a format:
//
//   TEXT     A text snippet (ASCII / UTF-8, no embedded '\0')
//   FILES    A list of file or folder paths, plus whether they were copied
//            or cut (explorer / desktop "Copy", "Cut", "Paste")
//   IMAGE    A 32 bpp ARGB bitmap (same layout as struct Image)
//   BINARY   Raw bytes, may contain '\0' (what the old dwm_clipboard_set
//            stored, so existing callers keep working)
//
// Every setter copies its input, so the caller's buffer can be reused right
// away. Every getter copies out, so the result stays valid after the
// clipboard changes. The only exception is the legacy dwm_clipboard_get,
// which returns a borrowed pointer (see below).
//
// Conversions: asking for TEXT also works when the clipboard holds FILES
// (the paths joined by '\n', so pasting a copied file into notepad pastes
// its path) or BINARY that contains no '\0' bytes.
//
// Every change bumps a sequence number. A window can cache it and compare
// later to see whether the clipboard changed (e.g. to enable "Paste"), and
// it lets a "cut" paste confirm it is finishing the cut it started.
//
// Threading: thread context only (the internal mutex may yield), never from
// an IRQ handler. Setters allocate from the kernel heap, so call them with
// the same lock held as any other malloc user (kernel_big_lock: the DWM and
// kernel event threads, and so every window procedure, already hold it).

#include <stdint.h>
#include <stdbool.h>

#include <kernel/dwm/configuration.h>
#include <kernel/dwm/rendering/image.h>

// Largest payload the clipboard accepts (the kernel heap is 4 MB)
#ifndef DWM_CLIPBOARD_MAX_BYTES
#define DWM_CLIPBOARD_MAX_BYTES     (512U * 1024U)
#endif

// Most paths a single FILES item may hold
#ifndef DWM_CLIPBOARD_MAX_FILES
#define DWM_CLIPBOARD_MAX_FILES     64
#endif

typedef enum {
    DWM_CLIPBOARD_EMPTY  = 0,
    DWM_CLIPBOARD_TEXT   = 1,
    DWM_CLIPBOARD_FILES  = 2,
    DWM_CLIPBOARD_IMAGE  = 3,
    DWM_CLIPBOARD_BINARY = 4,
} DWMClipboardFormat;

typedef enum {
    DWM_CLIPBOARD_OP_COPY = 0,      // Paste leaves the source in place
    DWM_CLIPBOARD_OP_CUT  = 1,      // Paste moves the source (see complete_cut)
} DWMClipboardFileOp;

// A consistent snapshot of what the clipboard holds
typedef struct {
    DWMClipboardFormat format;
    uint32_t           size;        // TEXT: length without '\0'; FILES: bytes of the
                                    // packed path list; IMAGE: width * height * 4;
                                    // BINARY: bytes
    uint32_t           file_count;  // FILES only
    DWMClipboardFileOp file_op;     // FILES only
    uint16_t           image_width; // IMAGE only
    uint16_t           image_height;
    uint32_t           sequence;
} DWMClipboardInfo;

//
// General
//

// Empty the clipboard (no-op when already empty)
void dwm_clipboard_clear(void);

DWMClipboardFormat dwm_clipboard_get_format(void);

// True when the contents can be read as `format`, including conversions
// (TEXT is available for TEXT, FILES and text-like BINARY)
bool dwm_clipboard_has_format(DWMClipboardFormat format);

// Changes every time the contents change (including clear)
uint32_t dwm_clipboard_get_sequence(void);

void dwm_clipboard_get_info(DWMClipboardInfo* out);

//
// Text snippets
//

// Copy a '\0'-terminated string. An empty string clears the clipboard.
// Returns false (and keeps the old contents) when too large or out of memory.
bool dwm_clipboard_set_text(const char* text);

// Same, for at most `length` characters (stops early at a '\0')
bool dwm_clipboard_set_text_n(const char* text, uint32_t length);

// Copy the clipboard as text into `out` (always '\0'-terminated when size > 0).
// Returns the full text length, like snprintf: a result >= size means the
// text was truncated. Call with (NULL, 0) to size a buffer. Returns 0 when
// nothing on the clipboard converts to text.
uint32_t dwm_clipboard_get_text(char* out, uint32_t size);

//
// File paths
//

// Put `count` paths on the clipboard. Each path must be non-empty and shorter
// than DWM_MAX_PATH_LEN. Fails (keeping the old contents) on any bad path.
bool dwm_clipboard_set_files(const char* const* paths, uint32_t count, DWMClipboardFileOp op);

// Convenience for a single path
bool dwm_clipboard_set_file(const char* path, DWMClipboardFileOp op);

// Number of paths, or 0 when the clipboard does not hold FILES
uint32_t dwm_clipboard_get_file_count(void);

// Copy path `index` into `out`. Returns false when there is no such path or
// it does not fit (out is then set to "").
bool dwm_clipboard_get_file(uint32_t index, char* out, uint32_t size);

// Finish a cut-and-paste. Call after the cut files were moved successfully,
// passing the sequence number read when the paste started. Clears the
// clipboard only if it still holds that same cut, so a newer copy made in
// the meantime is never wiped. Returns true when it cleared.
bool dwm_clipboard_files_complete_cut(uint32_t sequence);

//
// Images
//

// Copy a width x height ARGB bitmap
bool dwm_clipboard_set_image(const uint32_t* pixels, uint16_t width, uint16_t height);

// Copy the image into `out`, allocating out->data with malloc (the caller
// frees it). Returns false, with out->data = NULL, when there is no image.
bool dwm_clipboard_get_image(struct Image* out);

//
// Raw bytes (binary safe)
//

// Replace the clipboard with a copy of `size` bytes, stored as BINARY.
// size 0 (or data NULL) clears it.
bool dwm_clipboard_set(const void* data, uint32_t size);

// Copy the raw payload of TEXT or BINARY contents into `out`. Returns the
// full payload size (a result > size means it was truncated), or 0 for
// other formats.
uint32_t dwm_clipboard_get_data(void* out, uint32_t size);

// Legacy: borrowed pointer to the TEXT or BINARY payload, or NULL for other
// formats. Valid only until the next clipboard change, so copy it before
// releasing kernel_big_lock. Prefer dwm_clipboard_get_data / get_text.
const uint8_t* dwm_clipboard_get(uint32_t* out_size);

//
// File commands (what the Cut / Copy / Paste menu items call)
//
// These sit on top of the storage above, the VFS and the copy window
// (dwm/windows/filecopy.h). They show a message box themselves when
// something goes wrong, so a menu handler is one line. Same threading rules
// as everything else here, plus: never call them while holding the VFS lock
// (lock order is kernel_big_lock -> VFS lock).
//

// Put one existing file or folder on the clipboard for Copy or Cut.
// Refuses storage devices and paths longer than the clipboard can hold.
bool dwm_clipboard_put_file(const char* path, DWMClipboardFileOp op);

// Paste the clipboard into directory `dest_dir`:
//
//   FILES  opens a copy window that copies (for a cut: moves) every item
//          there in the background. Taken names get a " (2)" style suffix.
//          A cut that fully succeeds empties the clipboard. Items landing
//          in the desktop folder get icons, the first at (icon_x, icon_y)
//          or the next free spot when either is negative.
//   TEXT   writes the text to a new file named "clipping" right away.
//
// Returns true when something was pasted or a copy window was opened.
bool dwm_clipboard_paste_into(const char* dest_dir, int icon_x, int icon_y);

#endif
