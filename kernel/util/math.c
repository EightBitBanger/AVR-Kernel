#include <stdint.h>
#include <stddef.h>
#include <limits.h>

// Constants

#define M_PI       3.14159265358979323846
#define M_E        2.71828182845904523536
#define INFINITY   (__builtin_inff())
#define NAN        (__builtin_nanf(""))

// Integers

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

int clamp(int value, int min_val, int max_val) {
    return min(max(value, min_val), max_val);
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

// Exponentiation by squaring for integers
long long ipow(long long base, int exp) {
    long long res = 1;
    if (exp < 0) return (base == 1) ? 1 : ((base == -1) ? (exp % 2 ? -1 : 1) : 0);
    while (exp > 0) {
        if (exp & 1) res *= base;
        base *= base;
        exp >>= 1;
    }
    return res;
}

// Floating point (x87 Inline Assembly)


double sin(double x) {
    __asm__ __volatile__ ("fsin" : "+t" (x));
    return x;
}

double cos(double x) {
    __asm__ __volatile__ ("fcos" : "+t" (x));
    return x;
}

double tan(double x) {
    return sin(x) / cos(x);
}

double fabs(double x) {
    __asm__ __volatile__ ("fabs" : "+t" (x));
    return x;
}

double sqrt(double x) {
    if (x < 0) return NAN;
    __asm__ __volatile__ ("fsqrt" : "+t" (x));
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

// Arc tangent of y/x using full quadrant sign tracking
double atan2(double y, double x) {
    double res;
    __asm__ __volatile__ (
        "fpatan"
        : "=t" (res)
        : "0" (x), "u" (y)
        : "st(1)"
    );
    return res;
}

// Natural logarithm (ln) using instruction fyl2x: st(1) * log2(st(0))
double log(double x) {
    if (x <= 0.0) return (x == 0.0) ? -INFINITY : NAN;
    double res;
    __asm__ __volatile__ (
        "fldln2\n\t"
        "fxch\n\t"
        "fyl2x"
        : "=t" (res)
        : "0" (x)
        : "st(1)"
    );
    return res;
}

// Exponential function (e^x) leveraging x87 f2xm1 (calculates 2^frac - 1)
double exp(double x) {
    double res;
    double const l2e = 1.4426950408889634074; // log2(e)
    double scaled = x * l2e;
    
    double int_part = floor(scaled + 0.5);
    double frac_part = scaled - int_part;
    
    __asm__ __volatile__ (
        "f2xm1\n\t"       // st(0) = 2^frac_part - 1
        "fld1\n\t"        // st(0) = 1, st(1) = 2^frac_part - 1
        "faddp\n\t"       // st(0) = 2^frac_part
        "fscale"          // st(0) = 2^frac_part * 2^int_part
        : "=t" (res)
        : "0" (frac_part), "u" (int_part)
    );
    return res;
}

// Power function combining exp and log
double pow(double base, double exponent) {
    if (base < 0.0) {
        // Fractional powers of negative numbers produce imaginary numbers
        if (exponent == floor(exponent)) {
            long long e = (long long)exponent;
            double res = exp(exponent * log(-base));
            return (e % 2 == 0) ? res : -res;
        }
        return NAN;
    }
    if (base == 0.0) {
        if (exponent > 0.0) return 0.0;
        if (exponent < 0.0) return INFINITY;
        return 1.0;
    }
    return exp(exponent * log(base));
}
