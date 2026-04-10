#include <kernel/syscall/print.h>
#include <kernel/syscall/console.h>

#include <kernel/fs/fs.h>

#include <kernel/kernel.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

const char* msg_dir_not_found   = "Directory not found\n";
const char* msg_dir_error       = "Directory error\n";
const char* msg_dir_sym         = "<DIR>";
const char* msg_dir_root        = "/>";

const char* cmd_cd        = "cd";
const char* cmd_ls        = "ls";
const char* cmd_boot      = "boot";
const char* cmd_cls       = "cls";
const char* cmd_format    = "format";
const char* cmd_make      = "mk";
const char* cmd_makedir   = "mkdir";

const char* path_current  = ".";
const char* path_parent   = "..";
const char* path_root     = "/";

uint32_t current_directory = FS_NULL;
uint32_t mount_directory = FS_NULL;

void console_set_directory(uint32_t address) {
    current_directory = address;
}

uint32_t console_get_directory(void) {
    return current_directory;
}

static void update_prompt_from_path(uint32_t directory_address) {
    uint32_t path_stack[16];
    uint8_t  path_count = 0;
    
    char prompt[64];
    size_t offset = 0;
    
    if (directory_address == KMALLOC_NULL) {
        console_set_prompt("/>");
        return;
    }
    
    while (directory_address != KMALLOC_NULL && path_count < 16) {
        path_stack[path_count++] = directory_address;
        
        uint32_t parent = knode_get_parent(directory_address);
        if (directory_address == parent)
            break;
        
        directory_address = parent;
    }
    
    for (int i = path_count - 1; i >= 0; i--) {
        struct KernelDirectory object;
        
        memset(&object, 0x00, sizeof(struct KernelDirectory));
        kmem_read(path_stack[i], &object, sizeof(struct KernelDirectory));
        
        if (strcmp(object.name, "/") == 0) 
            continue;
        
        size_t name_len = strlen(object.name);
        if (name_len > 15) name_len = 15;
        
        memcpy(&prompt[offset], object.name, name_len);
        offset += name_len;
        
        if (i > 0) {
            prompt[offset++] = '/';
        }
    }
    
    prompt[offset++] = '>';
    prompt[offset]   = '\0';
    
    console_set_prompt(prompt);
}

static void update_prompt_from_directory(uint32_t directory_address) {
    struct KernelDirectory object;
    char prompt[19];
    size_t name_length;
    
    if (directory_address == KMALLOC_NULL) {
        console_set_prompt(msg_dir_root);
        return;
    }
    
    memset(&object, 0x00, sizeof(struct KernelDirectory));
    kmem_read(directory_address, &object, sizeof(struct KernelDirectory));
    
    if (strcmp(object.name, "/") == 0) {
        console_set_prompt("/>");
        return;
    }
    
    name_length = strlen(object.name);
    if (name_length > 15) {
        name_length = 15;
    }
    
    prompt[0] = '/';
    memcpy(&prompt[1], object.name, name_length);
    prompt[name_length + 1] = '>';
    prompt[name_length + 2] = '\0';
    
    console_set_prompt(prompt);
}

static void print_directory_path(uint32_t directory_address) {
    uint32_t path_stack[16];
    uint8_t  path_count = 0;
    uint8_t  index;
    
    if (directory_address == KMALLOC_NULL) {
        print("/\n");
        return;
    }
    
    while (directory_address != KMALLOC_NULL && path_count < 16) {
        path_stack[path_count] = directory_address;
        path_count++;
        
        if (directory_address == knode_get_parent(directory_address)) {
            break;
        }
        
        directory_address = knode_get_parent(directory_address);
    }
    
    if (path_count == 0) {
        print("/\n");
        return;
    }
    
    for (index = path_count; index > 0; index--) {
        struct KernelDirectory object;
        uint32_t address = path_stack[index - 1];
        
        memset(&object, 0x00, sizeof(struct KernelDirectory));
        kmem_read(address, &object, sizeof(struct KernelDirectory));
        
        if ((index == path_count) && (strcmp(object.name, "/") == 0)) {
            print("/");
            continue;
        }
        
        if (strcmp(object.name, "/") != 0) {
            print(object.name);
            
            if (index > 1) {
                print("/");
            }
        }
    }
    
    print("\n");
}

static void print_reference_entry(uint32_t address) {
    uint8_t flags = kmalloc_get_flags(address);
    uint8_t perms = kmalloc_get_permissions(address);
    
    char attr[5] = "    ";
    attr[4] = '\0';
    
    if (perms & KMALLOC_PERMISSION_EXECUTABLE) attr[0] = 'x';
    if (perms & KMALLOC_PERMISSION_READ)       attr[1] = 'r';
    if (perms & KMALLOC_PERMISSION_WRITE)      attr[2] = 'w';
    
    struct KernelDirectory kdir;
    memset(&kdir, 0x00, sizeof(struct KernelDirectory));
    kmem_read(address, &kdir, sizeof(struct KernelDirectory));
    
    print(attr);
    print(kdir.name);
    
    size_t name_len = strlen(kdir.name);
    size_t pad_len  = 0;
    
    if (display_get_width() > (name_len + 9)) {
        pad_len = display_get_width() - (name_len + 9);
    }
    
    size_t i;
    for (i = 0; i < pad_len; i++) {
        print(" ");
    }
    
    if (flags & KMALLOC_FLAG_DIRECTORY) {
        print(msg_dir_sym);
    }
    
    print("\n");
}

void process_command(char* keyboard_str) {
    struct Bus bus;
    bus.write_waitstate = 1;
    bus.read_waitstate = 2;
    
    char* args[16];
    int arg_count = 0;
    
    char* token = strtok(keyboard_str, " \n\t");
    while (token != NULL && arg_count < 16) {
        args[arg_count++] = token;
        token = strtok(NULL, " \n\t");
    }

    if (arg_count == 0) return;
    char* command = args[0];
    
    
    if (strcmp(command, cmd_cls) == 0) {
        console_busy_wait();
        console_clear();
        return;
    }
    
    if (strcmp(command, cmd_boot) == 0) {
        //if (mount_directory == FS_NULL) 
        //    return;
        //uint8_t bitmap[256];
        //struct FSHeaderBlock header;
        //fs_device_open(0x60000, bitmap, &header);
        
        uint32_t mount_device = knode_get_reference(mount_directory, 0);
        
        print_int( mount_device );
        
        return;
    }
    
    if (strcmp(args[0], cmd_format) == 0) {
        uint32_t total_capacity = 1024UL * 8UL;
        uint32_t sector_size    = 32UL;
        
        uint32_t bitmap_size = 256;
        uint8_t bitmap[bitmap_size];
        
        fs_device_format(0x60000, total_capacity, sector_size);
        
        struct FSHeaderBlock header;
        fs_device_open(0x60000, bitmap, &header);
        
        uint32_t root_directory = fs_directory_create("root", FS_PERMISSION_READ | FS_PERMISSION_WRITE);
        
        header.root_directory = root_directory;
        
        fs_mem_write(sizeof(struct FSDeviceHeader), &header, sizeof(struct FSHeaderBlock));
        
        fs_bitmap_flush();
    }
    
    if (strcmp(command, cmd_make) == 0) {
        uint8_t bitmap[256];
        struct FSHeaderBlock header;
        fs_device_open(0x60000, bitmap, &header);
        
        uint32_t file_address = fs_file_create(args[1], FS_PERMISSION_READ | FS_PERMISSION_WRITE);
        
        fs_directory_add_reference(header.root_directory, file_address);
        
        fs_bitmap_flush();
        return;
    }
    
    if (strcmp(command, cmd_makedir) == 0) {
        uint8_t bitmap[256];
        struct FSHeaderBlock header;
        fs_device_open(0x60000, bitmap, &header);
        
        uint32_t file_address = fs_directory_create(args[1], FS_PERMISSION_READ | FS_PERMISSION_WRITE);
        
        fs_bitmap_flush();
        return;
    }
    
    if ((strcmp(command, cmd_cd) == 0) || (strncmp(command, cmd_cd, 2) == 0)) {
        const char* cd_argument = args[1];
        
        if (strcmp(command, cmd_cd) != 0) {
            cd_argument = &command[2];
            
            if (cd_argument[0] == '\0') {
                cd_argument = args[1];
            }
        }
        
        if (cd_argument == NULL || cd_argument[0] == '\0') {
            return;
        }
        
        if (strcmp(cd_argument, path_current) == 0) {
            print_directory_path(current_directory);
            return;
        }
        
        uint32_t target_directory = KMALLOC_NULL;
        
        if (strcmp(cd_argument, path_root) == 0) {
            target_directory = knode_get_root();
        } else if (strcmp(cd_argument, path_parent) == 0) {
            target_directory = knode_get_parent(current_directory);
        } else {
            target_directory = knode_find_in(current_directory, cd_argument);
        }
        
        if (target_directory == KMALLOC_NULL) {
            print(msg_dir_not_found);
            return;
        }
        
        uint8_t flags = kmalloc_get_flags(target_directory);
        
        if (flags & KMALLOC_FLAG_MOUNT) {
            mount_directory = target_directory;
        } else {
            mount_directory = FS_NULL;
        }
        
        if (flags & KMALLOC_FLAG_DIRECTORY == 0) {
            print(msg_dir_error);
            return;
        }
        
        current_directory = target_directory;
        //update_prompt_from_directory(current_directory);
        update_prompt_from_path(current_directory);
        return;
    }
    
    if (strcmp(command, "ls") == 0) {
        uint32_t directory_address = current_directory;
        
        if (mount_directory != FS_NULL) {
            uint32_t reference_address = knode_get_reference(mount_directory, 0);
            if (reference_address == KMALLOC_NULL) 
                return;
            
            uint8_t bitmap[256];
            struct FSHeaderBlock headerBlock;
            fs_device_open(reference_address, bitmap, &headerBlock);
            
            //uint32_t reference = fs_directory_get_reference(headerBlock.root_directory, 0);
            //print_int(reference);
            
            for (uint32_t reference_index=0;;reference_index++) {
                uint32_t reference = fs_directory_get_reference(headerBlock.root_directory, reference_index);
                
                if (reference == FS_NULL) 
                    break;
                
                struct FSFileHeader header;
                fs_mem_read(reference, &header, sizeof(struct FSFileHeader));
                
                char attributes[] = "   \0";
                
                if (header.permissions & FS_PERMISSION_EXECUTE) attributes[0] = 'x';
                if (header.permissions & FS_PERMISSION_READ)    attributes[1] = 'r';
                if (header.permissions & FS_PERMISSION_WRITE)   attributes[2] = 'w';
                
                print(attributes);
                print(" ");
                print(header.name);
                
                if (header.attributes & FS_ATTRIBUTE_DIRECTORY) {
                    
                    size_t name_len = strlen(header.name);
                    size_t pad_len  = 0;
                    
                    if (display_get_width() > (name_len + 9)) {
                        pad_len = display_get_width() - (name_len + 9);
                    }
                    
                    size_t i;
                    for (i = 0; i < pad_len; i++) {
                        print(" ");
                    }
                    
                    print(msg_dir_sym);
                }
                
            }
            
            return;
        }
        
        for (uint32_t reference_index=0;;reference_index++) {
            uint32_t reference_address = knode_get_reference(directory_address, reference_index);
            
            if (reference_address == KMALLOC_NULL) {
                break;
            }
            
            print_reference_entry(reference_address);
        }
        
        return;
    }
}
