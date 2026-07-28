#ifndef _MATH_LIBRARY_H_
#define _MATH_LIBRARY_H_

#include <stdint.h>
#include <stddef.h>

int abs(int j);
int max(int a, int b);
int min(int a, int b);

int clamp(int value, int min_val, int max_val);
unsigned int isqrt(unsigned int num);
long long ipow(long long base, int exp);
uint32_t iatan2(int32_t y, int32_t x);

// Floating point

double sin(double x);
double cos(double x);
double tan(double x);

double fabs(double x);
double sqrt(double x);
double floor(double x);
double ceil(double x);
double atan2(double y, double x);

double log(double x);
double exp(double x);
double pow(double base, double exponent);

uint64_t __umoddi3(uint64_t num, uint64_t den);
uint64_t __udivdi3(uint64_t num, uint64_t den);
uint64_t __udivmoddi4(uint64_t num, uint64_t den, uint64_t *rem);

#endif
