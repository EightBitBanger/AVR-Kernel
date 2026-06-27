#include <kernel/kernel.h>

#include <kernel/console/virtual_key.h>
#include <kernel/console/console_const.h>
#include <kernel/console/display.h>
#include <kernel/console/console.h>
#include <kernel/console/print.h>
#include <kernel/syscall.h>

#include <kernel/util/string.h>
#include <kernel/util/parser.h>
#include <kernel/util/tok.h>

char* keyboard_string;
uint8_t keyboard_length;
uint8_t keyboard_length_max;

char* prompt_string;
uint8_t prompt_length;
uint8_t prompt_length_max;

void console_init(char* kb_string, char* kb_prompt, uint8_t kb_string_max_length, uint8_t kb_prompt_max_length) {
    keyboard_length_max = kb_string_max_length;
    prompt_length_max = kb_prompt_max_length;
    
    keyboard_string = kb_string;
    prompt_string = kb_prompt;
    
    strcpy(prompt_string, "/>");
    prompt_length = strlen(prompt_string);
    
    memset(keyboard_string, 0x00, keyboard_length_max);
    keyboard_length = 0;
    
    console_prompt_set_string("/>");
    
    display_clear();
}

void console_prompt_print(void) {
    print(prompt_string);
}

void console_prompt_set_string(const char* prompt) {
    if (strlen(prompt) >= prompt_length_max) 
        return;
    strcpy(prompt_string, prompt);
    prompt_length = strlen(prompt);
}

void console_get_keyboard_string(char* kb_string) {
    kb_string = keyboard_string;
}

uint8_t console_get_keyboard_string_length(void) {
    return keyboard_length;
}

void console_set_directory(uint32_t address) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    fs_current.current_directory = address;
    kernel_set_working_directory(&fs_current);
}

uint32_t console_get_directory(void) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    return fs_current.current_directory;
    return 0;
}

uint32_t console_get_mounted_device(void) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    return fs_current.mount_device;
    return 0;
}

uint32_t console_get_mounted_directory(void) {
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    return fs_current.mount_directory;
    return 0;
}

void console_get_path(char* path, uint16_t path_length, uint32_t knode_addr, uint32_t fs_addr, uint8_t depth) {
    uint32_t knode_stack[16];
    uint32_t fs_stack[16];
    uint8_t knode_count = 0, fs_count = 0;
    uint16_t offset = 0;
    
    if (!path || path_length == 0) return;
    if (depth == 0) {
        path[0] = '\0';
        return;
    }
    depth--;
    
    while (knode_addr != KMALLOC_NULL && knode_count < 16) {
        knode_stack[knode_count++] = knode_addr;
        uint32_t parent = knode_get_parent(knode_addr);
        if (parent == knode_addr) break;
        knode_addr = parent;
    }
    
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    if (fs_current.mount_device != FS_NULL && fs_addr != FS_NULL) {
        while (fs_addr != FS_NULL && fs_count < 16) {
            fs_stack[fs_count++] = fs_addr;
            uint32_t parent = fs_directory_get_parent(fs_addr);
            if (parent == fs_addr) break;
            fs_addr = parent;
        }
    }
    
    int total_elements = (knode_count > 0 ? knode_count - 1 : 0) + (fs_count > 0 ? fs_count - 1 : 0);
    int skipped = 0;
    int elements_to_skip = (total_elements > depth) ? (total_elements - depth) : 0;
    
    path[offset++] = '/';
    
    for (int i = knode_count - 1; i >= 0; i--) {
        struct KernelDirectory obj;
        kmem_read(&obj, knode_stack[i], sizeof(obj));
        
        if (obj.name[0] == '/' && obj.name[1] == '\0') continue;
        
        if (skipped < elements_to_skip) {
            skipped++;
            continue;
        }
        
        if (offset > 1 && offset < (path_length - 1)) 
            path[offset++] = '/';
        
        for (int j = 0; obj.name[j] && offset < (path_length - 1); j++) 
            path[offset++] = obj.name[j];
    }
    
    for (int i = fs_count - 2; i >= 0; i--) {
        if (skipped < elements_to_skip) {
            skipped++;
            continue;
        }
        
        struct FSFileHeader header;
        fs_mem_read(fs_stack[i], &header, sizeof(header));
        
        if (offset > 1 && offset < (path_length - 1)) 
            path[offset++] = '/';
        
        for (int j = 0; header.block.name[j] && offset < (path_length - 1); j++) 
            path[offset++] = header.block.name[j];
    }
    
    if (offset >= path_length) offset = path_length - 1;
    path[offset] = '\0';
}

void console_process_command(char* keyboard_str) {
    char* args[16];
    int arg_count = 0;
    char* command;
    
    parse_trim_leading_spaces(keyboard_str);
    
    if (keyboard_str[0] == ' ' || keyboard_str[0] == '\0') 
        return;
    
    cstr_tok_t tok;
    cstr_tok_init(&tok, keyboard_str, " \n\t");
    
    char* token = cstr_tok_next(&tok);
    while (token != NULL && arg_count < 16) {
        args[arg_count++] = token;
        token = cstr_tok_next(&tok);
    }
    
    command = args[0];
    // Check internal commands
    for (uint32_t i = 0; i < syscall_get_count(); i++) {
        CommandFunction function = syscall_get_function(i);
        const char* name = syscall_get_function_name(i);
        uint8_t flags = syscall_get_flags(i);
        
        if (!(flags & KSCF_COMMAND)) 
            continue;
        
        if (strcmp(command, name) != 0) 
            continue;
        
        function(arg_count-1, &args[1]);
        return;
    }
    
    if (syscall(SYSCALL_EXECUTE, args) != 0) {
        
        // Executable not found, check each path directory
        
        struct WorkingDirectory fs_current;
        struct LocalPaths fs_paths;
        kernel_get_working_directory(&fs_current);
        kernel_get_local_paths(&fs_paths);
        
        char old_path[PATH_LENGTH_MAX];
        console_get_path(old_path, PATH_LENGTH_MAX, fs_current.current_directory, fs_current.mount_directory, 16);
        
        char path[PATH_LENGTH_MAX];
        strcpy(path, fs_paths.path);
        
        int result = 1;
        
        cstr_tok_t tok;
        cstr_tok_init(&tok, path, ";");
        
        char* fs_path = cstr_tok_next(&tok);
        while (fs_path != NULL && result != 0) {
            
            syscall(SYSCALL_CHDIR, (char*[]){fs_path, NULL});
            
            result = syscall(SYSCALL_EXECUTE, args);
            
            syscall(SYSCALL_CHDIR, (char*[]){old_path, NULL});
            
            fs_path = cstr_tok_next(&tok);
        }
        
        if (result != 0) 
            print(msg_bad_command);
    }
    
    print("\n");
}

void console_print_reference_entry(uint32_t address) {
    if (address == KMALLOC_NULL) 
        return;
    
    uint8_t flags = kmalloc_get_flags(address);
    uint8_t perms = kmalloc_get_permissions(address);
    
    char permissions[5] = "    \0";
    if (perms & KMALLOC_PERMISSION_EXECUTABLE) permissions[0] = 'x';
    if (perms & KMALLOC_PERMISSION_READ)       permissions[1] = 'r';
    if (perms & KMALLOC_PERMISSION_WRITE)      permissions[2] = 'w';
    if (flags & KMALLOC_FLAG_MOUNT)            permissions[0] = 'm';
    
    struct KernelDirectory kdir;
    memset(&kdir, 0x00, sizeof(struct KernelDirectory));
    kmem_read(&kdir, address, sizeof(struct KernelDirectory));
    
    print(permissions);
    print(kdir.name);
    
    if (flags & KMALLOC_FLAG_DIRECTORY) {
        size_t attr_len = strlen(permissions);
        size_t name_len = strlen(kdir.name);
        
        for (size_t i = 0; i < (display_get_columns() - (attr_len + name_len) - attr_len - 1) / 2; i++) 
            print(" ");
        
        print(msg_dir_sym);
    }
    print("\n");
}

void console_print_fs_entry(uint32_t directory_address) {
    if (directory_address == FS_NULL) 
        return;
    
    struct WorkingDirectory fs_current;
    kernel_get_working_directory(&fs_current);
    
    struct FSPartitionBlock partition;
    fs_device_open(fs_current.mount_device, &partition);
    
    for (uint32_t reference_index=0;;reference_index++) {
        uint32_t reference = fs_directory_get_reference(fs_current.mount_directory, reference_index);
        if (reference == FS_NULL) 
            break;
        
        struct FSFileHeader header;
        fs_mem_read(reference, &header, sizeof(struct FSFileHeader));
        
        char permissions[5] = "    \0";
        if (header.block.permissions & FS_PERMISSION_EXECUTE) permissions[0] = 'x';
        if (header.block.permissions & FS_PERMISSION_READ)    permissions[1] = 'r';
        if (header.block.permissions & FS_PERMISSION_WRITE)   permissions[2] = 'w';
        
        print(permissions);
        print(header.block.name);
        
        size_t attr_len = strlen(permissions);
        size_t name_len = strlen(header.block.name);
        if (header.block.attributes & FS_ATTRIBUTE_DIRECTORY) {
            for (size_t i = 0; i < (display_get_columns() - (attr_len + name_len) - attr_len - 1) / 2; i++) 
                print(" ");
            
            print(msg_dir_sym);
        } else {
            uint32_t file_size = fs_file_get_size(reference);
            uint8_t size_len = 4;
            if (file_size > 9) size_len = 5;
            if (file_size > 99) size_len = 6;
            if (file_size > 999) size_len = 7;
            
            for (size_t i = 0; i < (display_get_columns() - (size_len + name_len) - 1) / 2; i++) 
                print(" ");
            
            print_int(file_size);
        }
        print("\n");
    }
}
