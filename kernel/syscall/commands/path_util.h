#ifndef SYSCALL_PATH_UTIL_H
#define SYSCALL_PATH_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/kernel.h>
#include <kernel/fs/fs.h>
#include <kernel/util/string.h>

// Resolve a command argument against the working directory.
//
// The VFS always resolves from the root, so a relative argument has to be
// prefixed with the working directory first. Some commands did this and some
// (rn, cp's destination, execute) didn't; the ones that did also turned an
// absolute argument into "<cwd>//abs". This helper does it one way everywhere:
//
//   "/mnt/ata0/file"  -> "/mnt/ata0/file"
//   "file"            -> "<cwd>/file"
static inline void command_resolve_path(const char* arg, char* out, size_t out_size) {
    if (out_size == 0) return;
    memset(out, '\0', out_size);
    if (arg == NULL) return;
    
    if (arg[0] == '/') {
        strncpy(out, arg, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    
    struct WorkingDirectory workingDirectory;
    kernel_get_working_directory(&workingDirectory);
    
    console_get_path(out, out_size, workingDirectory.current_directory, workingDirectory.mount_directory, out_size);
    
    // Kernel strncat takes the total buffer size (strlcat semantics)
    size_t len = strlen(out);
    if (len == 0 || out[len - 1] != '/') {
        strncat(out, "/", out_size);
    }
    strncat(out, arg, out_size);
}

#endif
