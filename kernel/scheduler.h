#ifndef __TASK_SCHEDULER_
#define __TASK_SCHEDULER_

#include <stdint.h>
#include <stddef.h>

#include <kernel/fs/fs.h>

#define TASK_NAME_LENGTH_MAX           10

#define TASK_TYPE_UNKNOWN              0   // No type defined
#define TASK_TYPE_TSR                  1   // Terminate and stay resident in memory
#define TASK_TYPE_VOLATILE             2   // Run once and terminate

#define TASK_PRIVILEGE_KERNEL          'k'
#define TASK_PRIVILEGE_SERVICE         's'
#define TASK_PRIVILEGE_USER            'u'

#define TASK_PRIORITY_HALT             0
#define TASK_PRIORITY_BACKGROUND       64
#define TASK_PRIORITY_LOW              32
#define TASK_PRIORITY_NORMAL           16
#define TASK_PRIORITY_HIGH             8
#define TASK_PRIORITY_REALTIME         1

#define EVENT_NOMESSAGE                0
#define EVENT_INITIATE                 1
#define EVENT_SHUTDOWN                 2

#define TASK_FLAG_INITIATE             0
#define TASK_FLAG_SHUTDOWN_RQ          1

// Process memory block ranges

#define MEMORY_START 0x4000
#define MEMORY_END 0xFFFF
#define MEMORY_BLOCK_SIZE 0x0400

#define yield() do { return; } while (0)


struct ProcessDescription {
	
	// Process name
	uint8_t  name[TASK_NAME_LENGTH_MAX];
	
	uint8_t  type;          // Process identification byte
	uint8_t  privilege;     // Memory access mode
	uint8_t  priority;      // Time reserved for the process
	uint8_t  counter;       // (Internal) priority counter
	uint8_t  flags;         // State flags
	
	// Address range of the processes heap
	uint32_t heap_begin;
	uint32_t heap_end;
	
	// Address pointer to the block of external memory allocated 
	// as a directory of references to memory allocated by this process
	uint32_t block;
	
	// Execution time slice
    uint16_t time_slice;
    
	// Pointer to the entry point of the task program
	void (*function)(uint8_t);
	
};


int32_t TaskCreate(uint8_t* name, uint8_t name_length, void(*task_ptr)(uint8_t), uint8_t priority, uint8_t privilege, uint8_t type);

uint8_t TaskDestroy(int32_t index);

int32_t TaskFind(uint8_t* name, uint8_t name_length);

struct ProcessDescription* GetProcInfo(int32_t index);

uint32_t FindNextAvailableMemoryRange(void);


void SchedulerStart(void);
void SchedulerStop(void);


void _ISR_SCHEDULER_MAIN__(void);
void _ISR_SCHEDULER_TIMER__(void);

#endif
