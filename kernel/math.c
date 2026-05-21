#include <stdint.h>
#include <stddef.h>
#include <limits.h>

int abs(int j) {
    int const mask = j >> (sizeof(int) * CHAR_BIT - 1);
    return (j + mask) ^ mask;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

int min(int a, int b) {
    return (a < b) ? a : b;
}

unsigned int isqrt(unsigned int num) {
    unsigned int res = 0;
    unsigned int bit = 1 << 30;
    
    while (bit > num) {
        bit >>= 2;
    }
    
    while (bit != 0) {
        if (num >= res + bit) {
            num -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

// Floating point

double fabs(double x) {
    __asm__ __volatile__ (
        "fabs"
        : "+t" (x)
    );
    return x;
}

double sin(double x) {
    __asm__ __volatile__ (
        "fsin"
        : "+t" (x)
    );
    return x;
}

double cos(double x) {
    __asm__ __volatile__ (
        "fcos"
        : "+t" (x)
    );
    return x;
}

double sqrt(double x) {
    if (x < 0) return 0.0 / 0.0;
    
    __asm__ __volatile__ (
        "fsqrt"
        : "+t" (x)
    );
    return x;
}

double floor(double x) {
    if (x >= 0) {
        return (double)((long long)x);
    } else {
        long long i = (long long)x;
        return (x == (double)i) ? x : (double)(i - 1);
    }
}

double ceil(double x) {
    if (x >= 0) {
        long long i = (long long)x;
        return (x == (double)i) ? x : (double)(i + 1);
    } else {
        return (double)((long long)x);
    }
}
