#ifndef CGEN_LIB_H
#define CGEN_LIB_H
#include <stdint.h>

typedef float T;
typedef uintptr_t I;
#define add(a,b) ((a)+(b))
#define sub(a,b) ((a)-(b))
#define mul(a,b) ((a)*(b))
#define div(a,b) ((a)/(b))
#define copy(a) (a)
#define zero() 0

// FIXME: This is not correct for negative numbers.
static inline T frac(T in) { return in - (I)in; }

#endif
