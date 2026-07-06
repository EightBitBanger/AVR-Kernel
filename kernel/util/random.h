#ifndef _RANDOM_NUMBER_GEN_LIBRARY_H_
#define _RANDOM_NUMBER_GEN_LIBRARY_H_

#include <stdint.h>
#include <stddef.h>

void rand_init(void);

int rand(void);

void rand_seed(unsigned int seed);

#endif
