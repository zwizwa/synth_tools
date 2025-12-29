// See test_fft.
// Originally implemented to abstract the FFT data type (comples or finite field).
#include <math.h>
struct NS(_complex) { float re; float im; };
typedef struct NS(_complex) NS(_data_t);
static inline void NS(_data_mul3)(NS(_data_t) *o,
                                  const NS(_data_t) *a,
                                  const NS(_data_t) *b) {
    o->re = a->re * b->re - a->im * b->im;
    o->im = a->re * b->im + a->im * b->re;
}
static inline void NS(_data_add2)(NS(_data_t) *o,
                                  const NS(_data_t) *a) {
    o->re += a->re;
    o->im += a->im;
}

static inline void NS(_init_coefs)(NS(_data_t) *c, int logn) {
    ASSERT(c);
    int n = 1<<logn;
    float dphase = (2 * M_PI) / ((float)n);
    for (int i=0; i<n; i++) {
        float phase = dphase * ((float)i);
        c[i].re = cosf(phase);
        c[i].im = sinf(phase);
    }
}
