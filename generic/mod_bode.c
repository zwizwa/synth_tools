#ifndef MOD_BODE
#define MOD_BODE

#include <math.h>
#include "macros.h"


typedef float complex_float[2];

/* Split off into mode_fft_bode. */

#define N 4096
#define SAMPLERATE 48000

/* See synth_tools.m fft_to_spectrum() */
struct bode {
    int offset_start;
    int offset_end;
    float f_0;
    float f_step;
    float db[N];
    float phase[N];
    float x_axis[N];
};

/* Compute the X-axis of a semilog plot.  We will not do any
   resampling.  Instead we compute the X-axis points corresponding to
   each sample point.

   The x_axis is in scaled units:
   - 0 = left of diagram
   - 1 = right of diagram

   offset_start / f_0     left   0.0
   offset_end   / f_max   right  1.0

   The x coordinate will then be:

   1.0f + log (f / f_0) / log(f_max / f_0)

*/
static void compute_semilog_axis(struct bode *bode) {
    float f_0 = bode->f_0;
    float f_range = ((float)bode->offset_end) / ((float)bode->offset_start);
    float f_max = f_0 * f_range;
    float scale = 1.0f / logf(f_range);
    float f = bode->f_0;
    for (int n=bode->offset_start; n<=bode->offset_end; n++) {
        float x = 1.0 + scale * logf(f / f_max);
        LOG("n=%d x=%f f=%f, db=%f ph=%f\n",
            n,
            x,
            f,
            bode->db[n],
            bode->phase[n]);
        f += bode->f_step;
    }
}

static void fft_to_spectrum(struct bode *bode, const complex_float *fft1) {

    /* The fftw output is not normalized, so scale it down. */
    float scale = 1.0f / sqrtf((float)N);

    for (int n=0; n<N; n++) {
        // a + ib
        float a = scale * fft1[n][0];
        float b = scale * fft1[n][1];
        float ampl = sqrtf(a*a + b*b);
        bode->db[n] = 20.0f * log10f(ampl);
        float rad = atan2f(b, a);
        bode->phase[n] = 180.0f * rad / M_PI;
    }

    bode->f_step = SAMPLERATE / N;
    // Limit the frequency range
    float f_left = 20;
    float f_right = SAMPLERATE / 2;
    bode->offset_start = roundf(f_left  / bode->f_step);
    bode->offset_end   = roundf(f_right / bode->f_step);
    bode->f_0 = bode->f_step * bode->offset_start;

    compute_semilog_axis(bode);
}





#endif
