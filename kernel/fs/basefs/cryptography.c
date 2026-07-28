#include <kernel/fs/fs.h>
#include <kernel/util/string.h>

bool fs_file_get_certificate(struct FSDeviceContext* ctx, uint32_t address, uint32_t* cert) {
    if (!ctx) return false;
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    *cert = header.block.certificate;
    return true;
}

bool fs_file_set_certificate(struct FSDeviceContext* ctx, uint32_t address, uint32_t cert) {
    if (!ctx) return false;
    if (cert == 0 || cert == 0xFFFFFFFF) 
        return false;
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    header.block.certificate = cert;
    fs_mem_write(ctx, address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_get_security_flag(struct FSDeviceContext* ctx, uint32_t address, uint8_t* cert) {
    if (!ctx) return false;
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    *cert = (uint8_t)header.block.certificate;
    return true;
}

bool fs_file_set_security_flag(struct FSDeviceContext* ctx, uint32_t address, uint32_t security) {
    if (!ctx) return false;
    struct FSFileHeader header;
    fs_mem_read(ctx, address, &header, sizeof(struct FSFileHeader));
    header.block.security = security;
    fs_mem_write(ctx, address, &header, sizeof(struct FSFileHeader));
    return true;
}
