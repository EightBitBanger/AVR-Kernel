#ifndef SYSCALL_COMMAND_SYSTEM_H
#define SYSCALL_COMMAND_SYSTEM_H

#define  SYSCALL_FLAG_COMMAND      1


#define  SYSCALL_EXECUTE           1

#define  SYSCALL_LIST              4
#define  SYSCALL_CHDIR             8
#define  SYSCALL_COPY             12

#define  SYSCALL_MAKE             16
#define  SYSCALL_MAKEDIR          20
#define  SYSCALL_REMOVE           24

#define  SYSCALL_BOOT             28
#define  SYSCALL_FORMAT           32
#define  SYSCALL_CHKDSK           34

#define  SYSCALL_TYPE             38
#define  SYSCALL_GRAPHICS         42


int syscall(uint16_t id, char** args);

typedef int (*CommandFunction)(int arg_count, char** args);

CommandFunction syscall_get_function(uint16_t index);
const char* syscall_get_function_name(uint16_t index);
uint8_t syscall_get_flags(uint16_t index);
uint16_t syscall_get_count(void);

#endif
