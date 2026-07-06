#include <kernel/fs/fs.h>
#include <kernel/util/string.h>

bool fs_file_get_certificate(uint32_t address, uint32_t* cert) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    *cert = header.block.certificate;
    return true;
}

bool fs_file_set_certificate(uint32_t address, uint32_t cert) {
    if (cert == 0 || cert == 0xFFFFFFFF) 
        return false;
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    header.block.certificate = cert;
    fs_mem_write(address, &header, sizeof(struct FSFileHeader));
    return true;
}

bool fs_file_get_security_flag(uint32_t address, uint8_t* cert) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    *cert = header.block.certificate;
    return true;
}

bool fs_file_set_security_flag(uint32_t address, uint32_t security) {
    struct FSFileHeader header;
    fs_mem_read(address, &header, sizeof(struct FSFileHeader));
    header.block.security = security;
    fs_mem_write(address, &header, sizeof(struct FSFileHeader));
    return true;
}
