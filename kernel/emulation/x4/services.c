#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/emulation/x4/x4.h>
#include <kernel/fs/fs.h>

void x4_read_string_from_dx(struct X4Thread* thread, char* str);

struct X4Pointer x4_pointer_from_low16(uint8_t low, uint8_t high) {
    struct X4Pointer pointer;
    pointer.address = ((uint32_t)low) | ((uint32_t)high << 8);
    return pointer;
}

void isc_operating_system(struct X4Thread* thread, uint8_t* program, uint8_t size, char** args, uint8_t arg_count) {}

void isc_display_routine(struct X4Thread* thread, char** args, uint8_t arg_count) {
    if (thread->cache.ah == 0x09) {
        
        for (uint32_t i = 0;; i++) {
            uint8_t buffer;
            struct X4Pointer dataOffset = x4_pointer_from_low16(thread->cache.dl, thread->cache.dh);
            kmalloc_read(thread->cache.cs + dataOffset.address + i, &buffer, 1);
            
            if (buffer == '\0')
                break;
            
            if (buffer == 0x0D) {
                print("\n");
                continue;
            }
            
            char ch[] = " ";
            ch[0] = buffer;
            print(ch);
        }
        
        thread->cache.state |= X4EMM_FLAG_CONSOLE_DIRTY;
        return;
    }
    
    if (thread->cache.ah == 0x03) {
        console_clear();
        console_set_cursor_position(0, 0);
        
        thread->cache.state &= ~X4EMM_FLAG_CONSOLE_DIRTY;
        return;
    }
    
    thread->cache.bl = 1;
}

void isc_filesystem_routine(struct X4Thread* thread, char** args, uint8_t arg_count) {
    uint32_t mount     = console_get_mounted_device();
    uint32_t directory = console_get_mounted_directory();
    if (mount == FS_NULL) 
        return;
    
    // File create
    if (thread->cache.ah == 0x3C) {
        struct X4Pointer file_size = x4_pointer_from_low16(thread->cache.cl, thread->cache.ch);
        
        char name[17];
        x4_read_string_from_dx(thread, name);
        
        uint8_t bitmap[256];
        uint8_t dirty[256];
        struct FSPartitionBlock partition;
        fs_device_open(mount, bitmap, dirty, &partition);
        
        uint32_t file_address = fs_file_create(name, FS_PERMISSION_READ | FS_PERMISSION_WRITE, file_size.address, directory);
        if (file_address == FS_NULL) {
            thread->cache.bl = 3;
            return;
        }
        
        fs_directory_add_reference(directory, file_address);
        
        fs_bitmap_flush();
        
        thread->cache.bl = 0;
        thread->cache.state &= ~X4EMM_FLAG_CONSOLE_DIRTY;
        
        return;
    }
    
    thread->cache.bl = 1;
}

void isc_network_routine(struct X4Thread* thread, char** args, uint8_t arg_count) {
    /*
    // AH = 0x0C Send a message
    if (reg[rAH] == 0x0C) {
        struct X4Pointer offset = x4_pointer_from_low16(reg[rDL], reg[rDH]);

        if (offset.address >= programSize || offset.address + reg[rCL] >= programSize) {
            reg[rBL] = 1;
            return;
        }

        switch (reg[rAL]) {
        case 0x00: ntSend(&programBuffer[offset.address], reg[rCL]); break;
        case 0x01: ntSend(param_string, param_length); break;
        }

        reg[rBL] = 0;
        return;
    }

    // AH = 0x0F Send a packet
    if (reg[rAH] == 0x0F) {
        struct X4Pointer offset = x4_pointer_from_low16(reg[rDL], reg[rDH]);

        if (offset.address >= programSize || offset.address + reg[rCL] >= programSize) {
            reg[rBL] = 1;
            return;
        }

        struct NetworkPacket packet;
        packet.start = NETWORK_PACKET_START_BYTE;
        packet.addr_d[0] = 0x00;
        packet.addr_d[1] = 0x00;
        packet.addr_s[0] = 0x00;
        packet.addr_s[1] = 0x00;
        memset(packet.data, 0x55, NETWORK_PACKET_DATA_SIZE);
        packet.index = 1;
        packet.total = 1;
        packet.stop = NETWORK_PACKET_STOP_BYTE;

        ntPacketSend(&packet);

        reg[rBL] = 0;
        return;
    }

    reg[rBL] = 1;
    */
}
