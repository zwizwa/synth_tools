/* Static wrapper for pffft code.

   This requires mod_pffft.c to be included in the C module so that
   pffft symbols are available.

   This wrapper can then be instantiated multiple times for different
   FFT sizes.

*/
struct NS(_static) {
    float MOD_PFFFT_ALIGN data[NS(_fft_size)];
    SETUP_STRUCT setup;
};

/* A buffer that can hold a real TD signal or a complex FD signal
   (half spectrum in pffft vector format) */
struct NS(_data) {
    float MOD_PFFFT_ALIGN data[NS(_fft_size)];
};





/* This replaces
   SETUP_STRUCT *FUNC_NEW_SETUP(int N, pffft_transform_t transform)
   from pffft/pffft_priv_impl.h
   with static allocation, i.e. a struct and an init function
   Only implemented for PFFFT_REAL
*/
void NS(_static_init)(struct NS(_static) *w) {
    SETUP_STRUCT *s = &w->setup;
    int N = NS(_fft_size);
    int k, m;

    s->N = N;
    s->transform = PFFFT_REAL;
    /* The PFFFT_REAL transform produces only half of the complex
       spectrum, so 256 complex bins for 512 bytes real input size.  These
       are then grouped in 4 float vectors on ARM Neon */
    s->Ncvec = (NS(_fft_size)/2)/SIMD_SZ;
    s->data = (v4sf *)&w->data[0];
    s->e = (float*)s->data;
    s->twiddle = (float *)(s->data + (2*s->Ncvec*(SIMD_SZ-1))/SIMD_SZ);

    for (k=0; k < s->Ncvec; ++k) {
        int i = k/SIMD_SZ;
        int j = k%SIMD_SZ;
        for (m=0; m < SIMD_SZ-1; ++m) {
            float A = -2*(float)M_PI*(m+1)*k / N;
            s->e[(2*(i*3 + m) + 0) * SIMD_SZ + j] = FUNC_COS(A);
            s->e[(2*(i*3 + m) + 1) * SIMD_SZ + j] = FUNC_SIN(A);
        }
    }
    rffti1_ps(N/SIMD_SZ, s->twiddle, s->ifac);

    /* check that N is decomposable with allowed prime factors */
    for (k=0, m=1; k < s->ifac[1]; ++k) { m *= s->ifac[2+k]; }
    if (m != N/SIMD_SZ) {
        ABORT;
    }
}
