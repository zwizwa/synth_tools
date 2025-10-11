/* Compute bode diagram */
#include "mod_bode.c"

/* FFT implementation. */
#include <fftw3.h>


#define DYNAMIC 1
#define N 4096
#define SAMPLERATE 48000

int main(int argc, char **argv) {

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

    return 0;
}
