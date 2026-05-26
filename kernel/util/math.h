#include <stdint.h>
#include <stddef.h>

int abs(int j);
int max(int a, int b);
int min(int a, int b);

unsigned int isqrt(unsigned int num);

#define clamp(val, lo, hi) ({           \
    __typeof__(val) _val = (val);       \
    __typeof__(lo) _lo = (lo);          \
    __typeof__(hi) _hi = (hi);          \
    _val < _lo ? _lo : (_val > _hi ? _hi : _val); \
})
