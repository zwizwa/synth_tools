#include <fftw3.h>
#define N 4096

#define DYNAMIC 1

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
    fftwf_execute(p); /* repeat as needed */
    fftwf_destroy_plan(p);

#if DYNAMIC
    fftwf_free(in);
    fftwf_free(out);
#endif

    return 0;
}
