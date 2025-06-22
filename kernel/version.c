#include <kernel/version.h>
#include <kernel/kconfig.h>
#include <kernel/command/cli.h>


void kPrintVersion(void) {
    
#ifdef _KERNEL_PRINT_VERSION_INFORMATION__
    
    ConsoleSetBlinkRate( CURSOR_BLINK_RATE );
    
    // Version
    uint8_t kernelHelloWorldString[] = "kernel";
    print(kernelHelloWorldString, sizeof(kernelHelloWorldString));
    printSpace(1);
    
    uint8_t versionMajor = _KERNEL_VERSION_MAJOR__;
    uint8_t versionMinor = _KERNEL_VERSION_MINOR__;
    uint8_t versionPatch = _KERNEL_VERSION_PATCH__;
    
    printChar('v');
    printInt(versionMajor);
    printChar('.');
    printInt(versionMinor);
    printChar('.');
    printInt(versionPatch);
    printLn();
    
#endif
    
}
