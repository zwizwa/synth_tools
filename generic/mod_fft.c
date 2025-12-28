/* Playground for hs/test-cproc.hs */

#ifndef MOD_FFT
#define MOD_FFT

/* Generated header */
#include "mod_fft.h"
#include "macros.h"
#include <math.h>
#include <stdint.h>

/* Power of two FFT.  The point here is to just have a dumb reference
   implementation that is optimized for code simplicity.

   Optimizations that are skipped in the first iteration:
   - Restrict to real data input
   - Restrict to half spectrum output
   - Re-use coefficients

   Do implement it in-place.

*/

struct complex { float re; float im; };

struct fft_data {
    struct complex c256[256];
    uint16_t br256[256];
};
static inline const struct complex *fft_coef(struct fft_data *x, int logn) {
    switch(logn) {
    case 8: return x->c256;
    default: return NULL;
    }
}
static inline const uint16_t *fft_br(struct fft_data *x, int logn) {
    switch(logn) {
    case 8: return x->br256;
    default: return NULL;
    }
}

struct fft_ctx {
    const struct complex *coef;
    int top_logn;
};

struct fft_sub {
    struct complex *vec;
    int logn;
};

static inline void complex_mul3(struct complex *o,
                                const struct complex *a,
                                const struct complex *b) {
    o->re = a->re * b->re - a->im * b->im;
    o->im = a->re * b->im + a->im * b->re;
}
static inline void complex_add2(struct complex *o,
                                const struct complex *a) {
    o->re += a->re;
    o->im += a->im;
}

static inline void fft_butterfly(struct complex *out,
                                 const struct complex *e,
                                 const struct complex *o,
                                 const struct complex *w) {
    complex_mul3(out, o, w);
    complex_add2(out, e);
}

void fft_p2_sub(const struct fft_ctx *x,
                const struct fft_sub *s) {
    if (s->logn == 0) {
        /* Degenerate case is the identity transform. */
        return;
    }
    else {
        /* Compute even sub-FFT */
        struct fft_sub s_sub = {
            .vec  = s->vec,
            .logn = s->logn-1,
        };
        fft_p2_sub(x, &s_sub);

        /* Compute odd sub-FFT, contiguous */
        int sub_n = 1 << s_sub.logn;
        s_sub.vec += sub_n;
        fft_p2_sub(x, &s_sub);

        /* Apply phase-shift butterfly. */
        for (int i=0; i<sub_n; i++) {

            /* Compute the butterfly given top and bottom elements. */
            struct complex *top = &s->vec[i+0];
            struct complex *bot = &s->vec[i+sub_n];

            /* Get the input values so we can write the output in-place. */
            struct complex e = *top;
            struct complex o = *bot;

            /* The stride is one at the top level, and doubles each level. */
            int w_stride = 1 << (x->top_logn - s->logn);
            const struct complex *w_top = &x->coef[w_stride * i];
            const struct complex *w_bot = &x->coef[w_stride * (i + sub_n)];

            fft_butterfly(top, &e, &o, w_top);
            fft_butterfly(bot, &e, &o, w_bot);

        }
    }


}

void fft_p2(struct fft_data *fft_data,
            const struct complex *in,
            struct complex *out,
            int logn)
{
    LOG("fft_p2\n");

    /* To make the algorithm a bit easier to express, permute the
       input to bit-reversed ordering such that all sub-FFTs are
       contigous and the end result has all frequency components in
       linear order.  The bit reversal is done using a pre-computed
       array. */
    const uint16_t *br = fft_br(fft_data, logn);
    int n = 1<<logn;
    for (int i=0; i<n; i++) {
        out[i] = in[br[i]];
    }
    struct fft_ctx x = {
        .top_logn = logn,
        .coef = fft_coef(fft_data, logn),
    };
    struct fft_sub s = {
        .vec  = out,
        .logn = logn,
    };
    fft_p2_sub(&x, &s);
}


void init_fft_coefs(struct fft_data *x, int logn) {
    struct complex *c = (typeof(c))fft_coef(x, logn);
    ASSERT(c);
    int n = 1<<logn;
    float dphase = (2 * M_PI) / ((float)n);
    for (int i=0; i<n; i++) {
        float phase = dphase * ((float)i);
        c[i].re = cosf(phase);
        c[i].im = sinf(phase);
    }
    uint16_t *br = (typeof(br))fft_br(x, logn);
    for (int i=0; i<n; i++) {
        uint16_t index = i;
        uint16_t indexr = 0;
        for (uint b=0; b<logn; b++) {
            indexr = (indexr << 1) | (index & 1);
            index >>= 1;
        }
        br[i] = indexr;
        // LOG("%3d %3d\n", i, indexr);
    }
}

void init_fft_data(struct fft_data *x) {
    memset(x,0,sizeof(*x));
    for (int logn=8; logn<=8; logn++) {
        LOG("init logn=%d\n", logn);
        init_fft_coefs(x, logn);
    }
}

void test_fft(void) {
    struct fft_data fft_data = {};
    init_fft_data(&fft_data);
}

#endif

