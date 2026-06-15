#ifndef MALLOC_H
#define MALLOC_H

#include <kernel/arch/x86/virtual/vmm.h>
#include <kernel/arch/x86/slab.h>

void* malloc(size_t size);

void free(void* ptr);

#endif
