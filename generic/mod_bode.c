#ifndef MOD_BODE
#define MOD_BODE

/* Compute bode plot from FFT.
   Modeled after octave/synth_tools.m fft_to_spectrum() */


#include <math.h>
#include "macros.h"


typedef float complex_float[2];

struct bode_vec {
    float db;
    float phase;
    float x_axis;
};
struct bode {
    int offset_start;
    int offset_end;
    float samplerate;
    float f_0;
    float f_step;
    float f_max;
    int size;
    struct bode_vec *vec;
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

   log (f / f_0) / log(f_max / f_0)

   What might be more useful is to normalize this to decades, since
   that will also be the background grid used in the SVG.  That would
   be:

   log (f / f_0) / log(10)

   0    20
   1   200
   2  2000
   3 20000

*/
static void bode_compute_semilog_axis(struct bode *bode) {
    float f_0 = bode->f_0;
    // float f_max = bode->f_max;
    // float scale = 1.0f / logf(f_max / f_0);
    float scale = 1.0f / logf(10);
    float f = bode->f_0;
    for (int n=bode->offset_start; n<=bode->offset_end; n++) {

        float x = scale * logf(f / f_0);
        if (0) {
            LOG("n=%d x=%f f=%f, db=%f ph=%f\n",
                n,
                x,
                f,
                bode->vec[n].db,
                bode->vec[n].phase);
        }
        f += bode->f_step;
        bode->vec[n].x_axis = x;
    }
}

static void bode_fft_to_spectrum(struct bode *bode, const complex_float *fft1) {

    /* The fftw output is not normalized, so scale it down. */
    float scale = 1.0f / sqrtf((float)bode->size);

    for (int n=0; n<bode->size; n++) {
        // a + ib
        float a = scale * fft1[n][0];
        float b = scale * fft1[n][1];
        float ampl = sqrtf(a*a + b*b);
        bode->vec[n].db = 20.0f * log10f(ampl);
        float rad = atan2f(b, a);
        bode->vec[n].phase = 180.0f * rad / M_PI;
    }

    bode->f_step = bode->samplerate / ((float)bode->size);
    // Limit the frequency range
    float f_left = 20;
    float f_right = bode->samplerate / 2;
    bode->offset_start = roundf(f_left  / bode->f_step);
    bode->offset_end   = roundf(f_right / bode->f_step);
    bode->f_0 = bode->f_step * bode->offset_start;

    float f_range = ((float)bode->offset_end) / ((float)bode->offset_start);
    bode->f_max = bode->f_0 * f_range;

    bode_compute_semilog_axis(bode);
}

/* Compute the SVG path string needed to plot the spectrum.

   M x y
   L x y
   L x y
   ...

*/
static int bode_svg_path_db_inner(const struct bode *bode, char sep,
                                  char *dst, int room) {
    char tag = 'M';
    int size = 0;
    for (int n=bode->offset_start; n<=bode->offset_end; n++) {
        int chunk =
            snprintf(
                dst, room,
                "%c %f %f%c",
                tag,
                bode->vec[n].x_axis,
                bode->vec[n].db, sep);
        // LOG("chunk = %d\n", chunk);
        tag = 'L';
        if (dst != NULL) {
            dst += chunk;
            room -= chunk;
        }
        size += chunk;
    }
    return size;

}
static char *bode_svg_path_db(const struct bode *bode, char sep) {
    int size = bode_svg_path_db_inner(bode, sep, NULL, 0);
    // LOG("size = %d\n", size);
    int room = size + 1;
    char *buf = malloc(room);
    bode_svg_path_db_inner(bode, sep, buf, room);
    // LOG("buf:\n%s\n", buf);
    return buf;
}



#endif
