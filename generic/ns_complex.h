// See test_fft.
// Originally implemented to abstract the FFT data type (comples or finite field).
#include <math.h>
struct NS(_complex) { float re; float im; };
typedef struct NS(_complex) NS(_data_t);
typedef float NS(_real_t);

static inline void NS(_data_mul3)(NS(_data_t) *o,
                                  const NS(_data_t) *a,
                                  const NS(_data_t) *b) {
    o->re = a->re * b->re - a->im * b->im;
    o->im = a->re * b->im + a->im * b->re;
}
static inline void NS(_data_mac3)(NS(_data_t) *acc,
                                  const NS(_data_t) *a,
                                  const NS(_data_t) *b) {
    acc->re += a->re * b->re - a->im * b->im;
    acc->im += a->re * b->im + a->im * b->re;
}
static inline void NS(_data_acc2)(NS(_data_t) *o,
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
static inline void NS(_from_real)(NS(_data_t) *o, const NS(_real_t) *a) {
    o->re = *a;
    o->im = 0;
}
static inline void NS(_to_real)(NS(_real_t) *o, const NS(_data_t) *a) {
    *o = a->re;
}

static inline void NS(_log_real_vec)(NS(_real_t) *v, int size) {
}
static inline void NS(_log_data_vec)(NS(_data_t) *v, int size) {
}
