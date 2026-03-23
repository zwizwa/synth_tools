#ifndef BODE_H
#define BODE_H

/* Compute bode plot from FFT.
   Modeled after octave/synth_tools.m fft_to_spectrum() */


#include <math.h>
#include "macros.h"


typedef float complex_float[2];

struct bode_vec {
    float db;
    float phase;
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

static inline void bode_fft_to_spectrum(struct bode *bode, const complex_float *fft1) {

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

}

/* Compute the SVG path string needed to plot the spectrum.

   M x y
   L x y
   L x y
   ...

*/
static inline int bode_svg_path_db_inner(const struct bode *bode,
                                         float pixels_per_decade,
                                         char sep,
                                         char *dst, int room) {


    /* This is the data we're fitting onto the grid. */
    float decade_left  = log10f(20);
    float decade_right = log10f(bode->samplerate/2);
    (void)decade_left;
    (void)decade_right;

    char tag = 'M';
    int size = 0;
    for (int n=bode->offset_start; n<=bode->offset_end; n++) {
        float f = ((float)n) * bode->f_step;
        float decade = log10f(f);

        /* The units are db=pixels, and parameterized number of pixels
           per decade.  A square "bode square" (the 20dB/decade first
           order rolloff) would be 20, but a more natural scale is
           60 which is a 3:1 aspect ratio for the semilog grid. */
        float x = decade * pixels_per_decade;
        float y = bode->vec[n].db;

        /* Here x,y are in decade, db coordinates.  It seems simpler
           to convert them to SVG coordiates here, given the box
           size. */
        int chunk = snprintf(dst, room, "%c %f %f%c", tag, x, y, sep);
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
static inline char *bode_svg_path_db(const struct bode *bode,
                                     float pixels_per_decade,
                                     char sep) {
    int size = bode_svg_path_db_inner(bode, pixels_per_decade, sep, NULL, 0);
    // LOG("size = %d\n", size);
    int room = size + 1;
    char *buf = malloc(room);
    bode_svg_path_db_inner(bode, pixels_per_decade, sep, buf, room);
    // LOG("buf:\n%s\n", buf);
    return buf;
}


/* Compute the SVG path string needed to plot an equidistant signal

   M x y
   L x y
   L x y
   ...

*/
static inline int svg_path_linear_inner(float *f, int stride, int nf,
                                        char sep,
                                        char *dst, int room) {
    char tag = 'M';
    int size = 0;
    for (int n=0; n<nf; n++) {
        float x = n;
        float y = f[n * stride];
        int chunk = snprintf(dst, room, "%c %f %f%c", tag, x, y, sep);
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
static inline char *svg_path_linear(float *f, int stride, int nf,
                                    char sep) {
    int size = svg_path_linear_inner(f, stride, nf, sep, NULL, 0);
    // LOG("size = %d\n", size);
    int room = size + 1;
    char *buf = malloc(room);
    svg_path_linear_inner(f, stride, nf, sep, buf, room);
    // LOG("buf:\n%s\n", buf);
    return buf;
}


#endif
