#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <kernel/delay.h>
#include <kernel/kernel.h>

#include <kernel/virtual/virtual.h>
#include <kernel/emulation/x4/x4.h>

// Program memory segmentation range
extern uint32_t __heap_begin__;
extern uint32_t __heap_end__;


// Program memory segment
uint8_t programBuffer[MAX_PROGRAM_SIZE];
uint32_t programSize;

// Parameters from the console
uint8_t param_string[32];
uint32_t param_length;

uint8_t reg[8];          // Registers
uint8_t stack[100];      // Temporary stack implementation

// Counters
uint32_t pc = 0;         // Program counter
uint32_t pcRet  = 0;     // Where to return from a call
uint32_t stack_ptr = 0;
// Flags
uint8_t flag_compare = 0;
uint8_t flag_greater = 0;
uint8_t flag_consoleDirty = 1;

// Interrupt service callbacks
void ISC_DisplayRoutine(void);
void ISC_NetworkRoutine(void);
void ISC_FileSystemRoutine(void);
void ISC_OperatingSystem(void);


void EmulatorSetProgram(uint8_t* buffer, uint32_t size) {
    memcpy(programBuffer, buffer, size);
    programSize = size;
    return;
}


void EmulatorSetParameters(uint8_t* parameters, uint32_t length) {
    memcpy(param_string, parameters, length);
    param_length = length;
    return;
}



void EmulateX4(uint8_t messages) {
    struct Bus mem_bus;
    mem_bus.read_waitstate = 2;
    mem_bus.write_waitstate = 2;
    
    pc = 0;
    pcRet  = 0;
    stack_ptr = 0;
    
    flag_compare = 0;
    flag_greater = 0;
    
    flag_consoleDirty = 1;
    
    VirtualAccessSetMode(VIRTUAL_ACCESS_MODE_USER);
    __heap_begin__ = __virtual_address_begin__;
    __heap_end__   = __virtual_address_end__;
    
    while(pc < MAX_PROGRAM_SIZE) {
        
        uint8_t opCode = programBuffer[pc];
        uint8_t argA   = programBuffer[pc + 1];
        uint8_t argB   = programBuffer[pc + 2];
        uint8_t argC   = programBuffer[pc + 3];
        uint8_t argD   = programBuffer[pc + 4];
        uint8_t argE   = programBuffer[pc + 5];
        
        pc++;
        
        switch (opCode) {
            
            // NOP
        case NOP:
            continue;
            
            // MOV read byte from memory
        case MOVMR: {
            // Set the address
            union Pointer ptr;
            ptr.byte_t[0] = argB;
            ptr.byte_t[1] = argC;
            ptr.byte_t[2] = argD;
            ptr.byte_t[3] = argE;
            
            // Check memory segmentation
            if (ptr.address > __heap_end__) 
                kThrow(HALT_SEGMENTATION_FAULT, __heap_begin__ + ptr.address);
            
            bus_read_memory(&mem_bus, __heap_begin__ + ptr.address, &reg[argA]);
            pc += 5;
            continue;
            }
            
            // MOV write byte into memory
        case MOVMW: {
            // Set the address
            union Pointer ptr;
            ptr.byte_t[0] = argB;
            ptr.byte_t[1] = argC;
            ptr.byte_t[2] = argD;
            ptr.byte_t[3] = argE;
            
            // Check memory segmentation
            if (ptr.address > __heap_end__) 
                kThrow(HALT_SEGMENTATION_FAULT, __heap_begin__ + ptr.address);
            
            bus_write_memory(&mem_bus, __heap_begin__ + ptr.address, reg[argA]);
            pc += 5;
            continue;
            }
            
            // MOV register into register
        case MOVR:
            reg[argA] = reg[argB];
            pc += 2;
            continue;
            
            // MOV byte into register
        case MOVB:
            reg[argA] = argB;
            pc += 2;
            continue;
            
            // MOV address into register as a word
        case MOVA:
            reg[argA]   = argB;
            reg[argA+1] = argC;
            pc += 5;
            continue;
            
            // Increment register
        case INC:
            reg[argA]++;
            pc += 1;
            continue;
            
            // Decrement register
        case DEC:
            reg[argA]--;
            pc += 1;
            continue;
            
            // Add BX into AX
        case ADD:
            reg[argA] += reg[argB];
            pc += 2;
            continue;
            
            // Sub BX from AX
        case SUB:
            reg[argA] -= reg[argB];
            pc += 2;
            continue;
            
            // Multiply
        case MUL:
            reg[argA] = reg[argB] * reg[argC];
            pc += 3;
            continue;
            
            // Divide
        case DIV:
            reg[argA] = reg[argB] / reg[argC];
            pc += 3;
            continue;
            
            // POP from stack into a register
        case POP:
            stack_ptr--;
            reg[argA] = stack[stack_ptr];
            pc += 1;
            continue;
            
            // PUSH to stack from a register
        case PUSH:
            stack[stack_ptr] = reg[argA];
            stack_ptr++;
            pc += 1;
            continue;
            
            // Bitwise
        case AND:
            reg[argA] = reg[argA] & reg[argB];
            pc += 2;
            continue;
            
        case OR:
            reg[argA] = reg[argA] | reg[argB];
            pc += 2;
            continue;
            
        case XOR:
            reg[argA] = reg[argA] ^ reg[argB];
            pc += 2;
            continue;
            
        case NOT:
            reg[argA] = ~reg[argA];
            pc += 2;
            continue;
            
            // Compare byte
        case CMP:
            if (reg[argA] == argB) {
                flag_compare = 1;
                flag_greater = 0;
            } else {
                flag_compare = 0;
                if (reg[argA] > argB) {
                    flag_greater = 1;
                } else {
                    flag_greater = 0;
                }
            }
            pc += 2;
            continue;
            
            // Compare registers
        case CMPR:
            if (reg[argA] == reg[argB]) {
                flag_compare = 1;
                flag_greater = 0;
            } else {
                flag_compare = 0;
                if (reg[argA] > reg[argB]) {
                    flag_greater = 1;
                } else {
                    flag_greater = 0;
                }
            }
            pc += 2;
            continue;
            
            // JUMP
        case JMP: {
            
            union Pointer ptr;
            ptr.byte_t[0] = argA;
            ptr.byte_t[1] = argB;
            ptr.byte_t[2] = argC;
            ptr.byte_t[3] = argD;
            
            pc = ptr.address;
            continue;
            }
            
            // Jump if EQUAL
        case JE:
            if (flag_compare == 1) {
                union Pointer ptr;
                ptr.byte_t[0] = argA;
                ptr.byte_t[1] = argB;
                ptr.byte_t[2] = argC;
                ptr.byte_t[3] = argD;
                pc = ptr.address;
                continue;
            }
            pc += 4;
            continue;
            
            // Jump if NOT EQUAL
        case JNE:
            if (flag_compare == 0) {
                union Pointer ptr;
                ptr.byte_t[0] = argA;
                ptr.byte_t[1] = argB;
                ptr.byte_t[2] = argC;
                ptr.byte_t[3] = argD;
                pc = ptr.address;
                continue;
            }
            pc += 4;
            continue;
            
            // Jump if GREATER than
        case JG:
            if (flag_greater == 1) {
                union Pointer ptr;
                ptr.byte_t[0] = argA;
                ptr.byte_t[1] = argB;
                ptr.byte_t[2] = argC;
                ptr.byte_t[3] = argD;
                pc = ptr.address;
                continue;
            }
            pc += 4;
            continue;
            
            // Jump if LESS than
        case JL:
            if (flag_greater == 0) {
                union Pointer ptr;
                ptr.byte_t[0] = argA;
                ptr.byte_t[1] = argB;
                ptr.byte_t[2] = argC;
                ptr.byte_t[3] = argD;
                pc = ptr.address;
                continue;
            }
            pc += 4;
            continue;
            
            // CALL
        case CALL: {
            union Pointer ptr;
            ptr.byte_t[0] = argA;
            ptr.byte_t[1] = argB;
            ptr.byte_t[2] = argC;
            ptr.byte_t[3] = argD;
            pcRet = pc + 4;
            pc = ptr.address;
            continue;
            }
            
            // RET return from call
        case RET:
            pc = pcRet;
            pcRet=0;
            continue;
            
            // Clear interrupt flag
        case CLI: {
            __asm__("cli");
            continue;
            }
            
            // Set interrupt flag
        case STI: {
            __asm__("sei");
            continue;
            }
            
        }
        
        // Interrupt routines
        if ((opCode == 0xCC) | (opCode == 0xCD)) {
            
            pc += 1;
            switch (argA) {
                
            case 0x10: ISC_DisplayRoutine(); break;
            case 0x13: ISC_FileSystemRoutine(); break;
            case 0x14: ISC_NetworkRoutine(); break;
            case 0x47: ISC_OperatingSystem(); break;
                
            }
            
            // Return control to the OS
            if (argA == 0x20) {
                if (flag_consoleDirty == 1) 
                    printLn();
                return;
            }
            
            continue;
        }
        
        continue;
    }
    
    VirtualAccessSetMode(VIRTUAL_ACCESS_MODE_KERNEL);
    __heap_begin__ = 0x00000000;
    __heap_end__   = 0xffffffff;
    
    if (flag_consoleDirty == 1) 
        printLn();
    
    return;
}


