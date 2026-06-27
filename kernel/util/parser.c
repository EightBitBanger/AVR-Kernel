#include <kernel/util/parser.h>

void parse_trim_leading_spaces(char *str) {
    if (str == NULL) return;
    int i = 0;
    while (str[i] == ' ' && str[i] != '\0') {i++;}
    
    if (i > 0) {
        int j = 0;
        while (str[i] != '\0') {str[j++] = str[i++];}
        str[j] = '\0';
    }
}

void parse_get_filename(const char* path, char* buffer, uint16_t buffer_size) {
    // Guard against null pointers or an empty buffer
    if (!path || !buffer || buffer_size == 0) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }
    
    const char* last_slash = NULL;
    const char* ptr = path;
    
    // Find the last occurrence of the '/' character
    while (*ptr != '\0') {
        if (*ptr == '/') {
            last_slash = ptr;
        }
        ptr++;
    }
    
    // Determine where the filename starts
    // If a slash was found, the filename starts right after it; otherwise, it starts at the beginning of path
    const char* filename_start = (last_slash != NULL) ? (last_slash + 1) : path;
    
    // Safely copy the filename to the buffer (mimicking strncpy behavior)
    uint16_t i = 0;
    while (filename_start[i] != '\0' && i < (buffer_size - 1)) {
        buffer[i] = filename_start[i];
        i++;
    }
    
    // Ensure the buffer is null-terminated
    buffer[i] = '\0';
}
