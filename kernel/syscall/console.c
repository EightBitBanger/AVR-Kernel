#include <kernel/kernel.h>
#include <kernel/boot/avr/interrupt.h>

#include <kernel/syscall.h>

#include <kernel/syscall/console_const.h>
#include <kernel/syscall/console.h>

#include <kernel/scheduler/scheduler.h>
#include <kernel/syscall/print.h>
#include <kernel/fs/fs.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint32_t display_address;

uint8_t display_width;
uint8_t display_height;

uint8_t console_position;
uint8_t console_line;

char* keyboard_string;
uint8_t keyboard_length = 0;

char* prompt_string;
uint8_t prompt_length = 0;

void print_fs_entry(uint32_t directory_address);
void print_reference_entry(uint32_t address);

void console_set_directory(uint32_t address) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    fs_current.current_directory = address;
    kernel_set_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
}

uint32_t console_get_directory(void) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    return fs_current.current_directory;
}

uint32_t console_get_mounted_device(void) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    return fs_current.mount_device;
}

uint32_t console_get_mounted_directory(void) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    return fs_current.mount_directory;
}


void console_init(char* kb_string, char* kb_prompt) {
    display_width  = 21;
    display_height = 8;
    
    console_position = 0;
    console_line = 0;
    
    keyboard_string = kb_string;
    prompt_string = kb_prompt;
    
    strcpy(prompt_string, "/>");
    prompt_length = strlen(prompt_string);
    
    memset(keyboard_string, 0x00, KEYBOARD_STRING_LENGTH);
    keyboard_length = 0;
}



void parser_trim_leading_spaces(char *str) {
    if (str == NULL) return;
    
    int i = 0;
    // 1. Find the first character that isn't a space
    while (str[i] == ' ' && str[i] != '\0') {
        i++;
    }

    // 2. If there were leading spaces, shift everything left
    if (i > 0) {
        int j = 0;
        while (str[i] != '\0') {
            str[j++] = str[i++];
        }
        str[j] = '\0'; // Null-terminate at the new end
    }
}


void console_process_command(char* keyboard_str) {
    char* args[16];
    int arg_count = 0;
    char* command;
    
    parser_trim_leading_spaces(keyboard_str);
    
    if (keyboard_str[0] == ' ' || keyboard_str[0] == '\0') 
        return;
    
    char* token = strtok(keyboard_str, " \n\t");
    while (token != NULL && arg_count < 16) {
        args[arg_count++] = token;
        token = strtok(NULL, " \n\t");
    }
    command = args[0];
    
    // Special CD handling
    if ((strcmp(command, "cd") == 0) || (strncmp(command, "cd", 2) == 0)) {
        char* cd_argument = NULL;
        if (strcmp(command, "cd") == 0) {
            cd_argument = args[1];
        } else {
            cd_argument = &command[2];
            
            if (cd_argument[0] == '\0') {
                cd_argument = args[1];
            }
        }
        if (cd_argument == NULL || cd_argument[0] == '\0') 
            return;
        args[0] = cd_argument;
        
        if (arg_count < 2) 
            arg_count = 2;
        
        syscall(SYSCALL_CHDIR, args);
        return;
    }
    
    // Check internal commands
    for (uint32_t i = 0; i < syscall_get_count(); i++) {
        CommandFunction function = syscall_get_function(i);
        const char* name = syscall_get_function_name(i);
        uint8_t flags = syscall_get_flags(i);
        
        if (!(flags & SYSCALL_FLAG_COMMAND)) 
            continue;
        
        if (strcmp(command, name) != 0) 
            continue;
        
        function(arg_count-1, &args[1]);
        return;
    }
    
    if (syscall(SYSCALL_EXECUTE, args) != 0) {
        
        // Executable not found, check each path directory
        
        struct WorkingDirectory fs_current;
        kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
        
        char old_path[32];
        console_get_path(old_path, 32, fs_current.current_directory, fs_current.mount_directory, 16);
        
        char path[32];
        strcpy(path, fs_current.path);
        
        int result = 1;
        char* fs_path = strtok(path, ";");
        while (fs_path != NULL && result != 0) {
            
            syscall(SYSCALL_CHDIR, (char*[]){fs_path, NULL});
            
            result = syscall(SYSCALL_EXECUTE, args);
            
            syscall(SYSCALL_CHDIR, (char*[]){old_path, NULL});
            
            fs_path = strtok(NULL, ";");
        }
        
        if (result != 0) 
            print(msg_bad_command);
    }
    
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
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
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
        kmalloc_read(knode_stack[i], &obj, sizeof(obj));
        
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

void print_reference_entry(uint32_t address) {
    uint8_t flags = kmalloc_get_flags(address);
    uint8_t perms = kmalloc_get_permissions(address);
    
    char permissions[5] = "    \0";
    if (perms & KMALLOC_PERMISSION_EXECUTABLE) permissions[0] = 'x';
    if (perms & KMALLOC_PERMISSION_READ)       permissions[1] = 'r';
    if (perms & KMALLOC_PERMISSION_WRITE)      permissions[2] = 'w';
    if (flags & KMALLOC_FLAG_MOUNT)            permissions[0] = 'm';
    
    struct KernelDirectory kdir;
    memset(&kdir, 0x00, sizeof(struct KernelDirectory));
    kmalloc_read(address, &kdir, sizeof(struct KernelDirectory));
    
    print(permissions);
    print(kdir.name);
    
    if (flags & KMALLOC_FLAG_DIRECTORY) {
        size_t attr_len = strlen(permissions);
        size_t name_len = strlen(kdir.name);
        
        for (size_t i = 0; i < display_get_width() - (attr_len + name_len) - attr_len - 1; i++) 
            print(" ");
        
        print(msg_dir_sym);
    }
    
    print("\n");
}

void print_fs_entry(uint32_t directory_address) {
    struct WorkingDirectory fs_current;
    kernel_get_system_object(&fs_current, KSO_WORKING_DIRECTORY, sizeof(struct WorkingDirectory));
    
    uint8_t bitmap[256];
    uint8_t dirty[256];
    struct FSPartitionBlock partition;
    fs_device_open(fs_current.mount_device, bitmap, dirty, &partition);
    
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
            for (size_t i = 0; i < display_get_width() - (attr_len + name_len) - attr_len - 1; i++) 
                print(" ");
            
            print(msg_dir_sym);
        } else {
            uint32_t file_size = fs_file_get_size(reference);
            uint8_t size_len = 4;
            if (file_size > 9) size_len = 5;
            if (file_size > 99) size_len = 6;
            if (file_size > 999) size_len = 7;
            
            for (size_t i = 0; i < display_get_width() - (size_len + name_len) - 1; i++) 
                print(" ");
            
            print_int(file_size);
        }
        
        print("\n");
    }
}

void console_busy_wait(void) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    uint8_t checkByte = 0;
    mmio_readb(&bus, display_address, &checkByte);
    while (checkByte != 0x10) {
        mmio_readb(&bus, display_address, &checkByte);
    }
}

uint16_t console_get_position(void) {
    return console_position;
}

uint16_t console_get_line(void) {
    return console_line;
}

void console_set_position(uint16_t position) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    console_position = position;
    mmio_writeb(&bus, display_address + 0x00002, &console_position);
}

void console_set_line(uint16_t line) {
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    console_line = line;
    mmio_writeb(&bus, display_address + 0x00001, &console_line);
}

void console_set_cursor_position(uint8_t position, uint8_t line) {
    console_busy_wait();
    
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    // Boundary checks to prevent memory corruption/glitches
    if (position >= display_width)  position = display_width - 1;
    if (line     >= display_height) line     = display_height - 1;
    
    console_set_position(position);
    console_set_line(line);
}


void console_set_prompt(const char* prompt) {
    strcpy(prompt_string, prompt);
    prompt_length = strlen(prompt);
}

void console_set_blink_rate(uint8_t rate) {
    console_busy_wait();
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    mmio_writeb(&bus, display_address + 0x00003, &rate);
}

void console_clear(void) {
    console_busy_wait();
    
    struct Bus bus;
    bus.write_waitstate = 20;
    bus.read_waitstate  = 20;
    
    console_set_cursor_position(0, 0);
    for (uint8_t h=0; h < display_height; h++) 
        for (uint8_t w=0; w < display_width; w++) 
            print(" ");
    console_set_cursor_position(0, 0);
    return;
}

uint16_t display_get_width(void) {
    return display_width;
}

uint16_t display_get_height(void) {
    return display_height;
}
