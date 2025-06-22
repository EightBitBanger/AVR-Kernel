#include <kernel/fs/fs.h>
#include <kernel/syscalls/print/print.h>

uint8_t vfsList(struct Partition part, DirectoryHandle handle) {
    uint8_t msgDir[] = "<dir>";
    
    uint32_t directoryRefTotal = fsDirectoryGetTotalSize(part, handle);
    if (directoryRefTotal == 0) 
        return 0;
    
    for (uint32_t i=0; i < directoryRefTotal; i++) {
        
        uint32_t entryHandle = fsDirectoryFindByIndex(part, handle, i);
        
        // Print attributes
        uint8_t fileAttr[] = "    ";
        fsFileGetAttributes(part, entryHandle, fileAttr);
        
        // Check directory or file
        uint8_t isDirectory = 0;
        if (fileAttr[3] == 'd') 
            isDirectory = 1;
        
        print(fileAttr, 4);
        printSpace(1);
        
        // Print file name
        uint8_t filename[] = "          ";
        fsFileGetName(part, entryHandle, filename);
        print(filename, sizeof(filename));
        printSpace(1);
        
        // Print file size / dir
        if (isDirectory == 1) {
            print(msgDir, sizeof(msgDir));
        } else {
            uint32_t fileSize = fsFileGetSize(part, entryHandle);
            printInt(fileSize);
        }
        printLn();
    }
    
    return 0;
}
