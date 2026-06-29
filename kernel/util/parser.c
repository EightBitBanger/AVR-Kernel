#include <kernel/util/parser.h>
#include <kernel/util/string.h>

void parse_trim_leading_spaces(char* str) {
    if (!str) 
        return;
    int i = 0;
    while (str[i] == ' ' && str[i] != '\0') {i++;}
    
    if (i > 0) {
        int j = 0;
        while (str[i] != '\0') {str[j++] = str[i++];}
        str[j] = '\0';
    }
}

void parse_get_filename(const char* path, char* buffer, uint16_t buffer_size) {
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
    
    // Copy the filename to the buffer
    uint16_t i = 0;
    while (filename_start[i] != '\0' && i < (buffer_size - 1)) {
        buffer[i] = filename_start[i];
        i++;
    }
    
    // Ensure the buffer is null-terminated
    buffer[i] = '\0';
}

void parse_get_parent_path(const char* path, char* buffer, uint16_t buffer_size) {
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
    
    // Determine the length of the parent path
    uint16_t copy_len = 0;
    if (last_slash != NULL) {
        if (last_slash == path) {
            // Path is in the root directory (e.g., "/file" -> parent is "/")
            copy_len = 1;
        } else {
            // Path has a nested parent (e.g., "/mnt/file" -> parent is "/mnt")
            copy_len = last_slash - path;
        }
    } else {
        // No slash found (e.g., "file" -> parent is current directory "")
        copy_len = 0;
    }
    
    // Ensure we don't overflow the buffer (leave room for the null-terminator)
    if (copy_len >= buffer_size) {
        copy_len = buffer_size - 1;
    }
    
    // Copy the parent path into the buffer
    for (uint16_t i = 0; i < copy_len; i++) {
        buffer[i] = path[i];
    }
    
    // Ensure the buffer is null-terminated
    buffer[copy_len] = '\0';
}

int str_replace(char *dest, const char *to_find, const char *to_replace, int max_size) {
    // Find the substring in the destination string
    char *match = strstr(dest, to_find);
    
    // If the substring to find isn't in the destination, exit early
    if (match == NULL) {
        return -1; // Error: Substring not found
    }
    
    int dest_len = strlen(dest);
    int find_len = strlen(to_find);
    int replace_len = strlen(to_replace);
    
    // Calculate the final size and ensure it fits in the buffer
    // Final size = current length - old substring length + new substring length
    if (dest_len - find_len + replace_len >= max_size) {
        return -2; // Error: Not enough buffer space
    }
    
    // Allocate a temporary buffer to build our new string
    char temp[max_size];
    memset(temp, 0, max_size);
    
    // Copy the prefix (everything before the match)
    int prefix_len = match - dest;
    strncpy(temp, dest, prefix_len);
    
    // Append the replacement string
    strcat(temp, to_replace);
    
    // Append the suffix (everything after the matched substring)
    // match + find_len skips past the old word completely
    strcat(temp, match + find_len);
    
    // Copy the finalized string back into the original destination buffer
    strcpy(dest, temp);
    
    return 0; // Success
}

void str_tolower(char *str) {
    if (!str) 
        return;
    
    while (*str != '\0') {
        if (*str >= 'A' && *str <= 'Z') {
            *str = *str + 32;
        }
        str++;
    }
}

void str_toupper(char *str) {
    if (!str) 
        return;
    
    while (*str != '\0') {
        if (*str >= 'a' && *str <= 'z') {
            *str = *str - 32;
        }
        str++;
    }
}
