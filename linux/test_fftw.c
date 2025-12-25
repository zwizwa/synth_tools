/* Compute bode diagram */
#include "mod_bode.c"

/* FFT implementation. */
#include <fftw3.h>


#define DYNAMIC 1
#define N 4096
#define SAMPLERATE 48000

void test1(void) {

#if DYNAMIC
    fftwf_complex *in, *out;
    in = (fftwf_complex*) fftwf_malloc(sizeof(fftw_complex) * N);
    out = (fftwf_complex*) fftwf_malloc(sizeof(fftw_complex) * N);
#else
    fftwf_complex in[N], out[N];
#endif

    fftwf_plan p;
    p = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    for (int n=0; n<N; n++) {
        in[n][0] = 1.0f / (((float)n)+1.0f);
        in[n][1] = 0.0f;
    }
    fftwf_execute(p); /* repeat as needed */

    struct bode_vec bode_vec[N];
    struct bode bode = {
        .samplerate = 48000,
        .size = N,
        .vec = bode_vec,
    };
    bode_fft_to_spectrum(&bode, out);

    char sep = '\n';
    float pixels_per_decade = 40;
    char *path = bode_svg_path_db(&bode, pixels_per_decade, sep);
    LOG("path:\n%s\n", path);

    free(path);


    fftwf_destroy_plan(p);

#if DYNAMIC
    fftwf_free(in);
    fftwf_free(out);
#endif

}

// Overlap-add convolution
//
// 1. Assume fir length is F
// 2. Assume signal block size is B
// 3. Convolution length is F+B-1  (we can discard the -1 to simplify calc)
//
// 4. To use a circular convolution, the FFT size needs to e >= F+B-1
//    so there is no time-wraparound
//
// 5. OLA: To split up in different blocks: add the tail of one
//    convolution to the head of the next.
//
// 6. OLS: Discard the tail = the part that is incomplete, and
//    incrementally shift the input.
//
// 7. Algorithm gets more efficient as FFT size increases.
//
// 8. Algorithm accrues more delay as B increases.
//
//



void test2(void) {
}

void test3(void) {
    // Overlap-save convolution
}

int main(int argc, char **argv) {
    //test1();
    test2();
    test3();
    return 0;
}

