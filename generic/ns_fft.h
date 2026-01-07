/* Power of two FFT.  The point here is to just have a dumb reference
   implementation that can be used to avoid external dependencies, and
   is implemented with the base field abstract.  See test_fft.c for
   complex and galois field implemenetation (for exact testing).

   Optimizations that are skipped:
   - Restrict to real data input
   - Restrict to half spectrum output
   - Make use of coefficient symmetry
*/

#include "macros.h"
#include <math.h>
#include <stdint.h>

/* Generated header */
#include "ns_fft_gen.h"

struct NS(_ctx) {
    NS(_data_t) coef[1<<NS(_logn)];
    uint16_t bitrev[1<<NS(_logn)];
    int top_logn;
    int direction; // 1 use coefs: -1: use reversed coefs
    NS(_data_t) scale;
};

struct NS(_sub) {
    NS(_data_t) *vec;
    int logn;
};

static inline void NS(_butterfly)(NS(_data_t) *out,
                                  const NS(_data_t) *e,
                                  const NS(_data_t) *o,
                                  const NS(_data_t) *w) {
    NS(_data_mul3)(out, o, w);
    NS(_data_acc2)(out, e);
}

void NS(_sub)(const struct NS(_ctx) *x,
              const struct NS(_sub) *s) {
    if (s->logn == 0) {
        /* Degenerate case is the identity transform. */
        return;
    }
    else {
        /* Compute even sub-FFT */
        struct NS(_sub) s_sub = {
            .vec  = s->vec,
            .logn = s->logn-1,
        };
        NS(_sub)(x, &s_sub);

        /* Compute odd sub-FFT, contiguous */
        int sub_n = 1 << s_sub.logn;
        s_sub.vec += sub_n;
        NS(_sub)(x, &s_sub);

        /* Apply phase-shift butterfly. */
        for (int i=0; i<sub_n; i++) {

            /* Compute the butterfly given top and bottom elements. */
            NS(_data_t) *top = &s->vec[i+0];
            NS(_data_t) *bot = &s->vec[i+sub_n];

            /* Get the input values so we can write the output in-place. */
            NS(_data_t) e = *top;
            NS(_data_t) o = *bot;

            /* The stride is one at the top level, and doubles each level. */
            int w_stride = x->direction << (x->top_logn - s->logn);
            uint32_t mask = (1 << x->top_logn)-1;
            const NS(_data_t) *w_top = &x->coef[mask & (w_stride * i)];
            const NS(_data_t) *w_bot = &x->coef[mask & (w_stride * (i + sub_n))];

            NS(_butterfly)(top, &e, &o, w_top);
            NS(_butterfly)(bot, &e, &o, w_bot);

        }
    }


}

void NS(_process)(const struct NS(_ctx) *ctx,
                  const NS(_data_t) *in,
                  NS(_data_t) *out)
{
    /* To make the algorithm a bit easier to express, permute the
       input to bit-reversed ordering such that all sub-FFTs are
       contigous and the end result has all frequency components in
       linear order.  The bit reversal is done using a pre-computed
       array. */
    const uint16_t *br = ctx->bitrev;
    int n = 1<<ctx->top_logn;
    for (int i=0; i<n; i++) {
        /* Perform the scaling as well. */
        NS(_data_mul3)(&out[i], &ctx->scale,  &in[br[i]]);
    }

    /* Prepare global context and sub-step context to start
       recursion. */
    struct NS(_sub) s = {
        .vec  = out,
        .logn = ctx->top_logn,
    };
    NS(_sub)(ctx, &s);
}

/* Same as NS(_process) but take input from a real array.
   Only meaningful for float complex data. */
void NS(_process_real)(const struct NS(_ctx) *ctx,
                       const NS(_real_t) *in,
                       NS(_data_t) *out)
{
    const uint16_t *br = ctx->bitrev;
    int n = 1<<ctx->top_logn;
    for (int i=0; i<n; i++) {
        NS(_data_t) d;
        NS(_from_real)(&d, in[br[i]]);
        NS(_data_mul3)(&out[i], &ctx->scale, &d);
    }
    struct NS(_sub) s = {
        .vec  = out,
        .logn = ctx->top_logn,
    };
    NS(_sub)(ctx, &s);
}



void NS(_init_bitrev)(struct NS(_ctx) *x, int logn) {
    uint16_t *br = x->bitrev;
    int n = 1<<logn;
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

void NS(_dir_fwd)(struct NS(_ctx) *x) {
    x->direction = -1;
    x->scale = NS(_scale_fwd);
}
void NS(_dir_rev)(struct NS(_ctx) *x) {
    x->direction = +1;
    x->scale = NS(_scale_rev);
}

void NS(_init_ctx)(struct NS(_ctx) *x) {
    memset(x,0,sizeof(*x));
    x->top_logn = NS(_logn);
    NS(_dir_fwd)(x);
    LOG("init logn = %d\n", x->top_logn);
    NS(_data_t) *c = x->coef;
    NS(_init_coefs)(c, x->top_logn);
    NS(_init_bitrev)(x, x->top_logn);
}



