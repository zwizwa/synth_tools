/* This module wraps the pffft library from
   https://github.com/marton78/pffft
   d321d006467fcdcafc4298901c27541a18da598c
   And provides:

   - replacement initialization function and struct for static data allocation

   - direct inclusion of source files to avoid extra .o .a and provide
     more optimalization opportunity

   - delay line for partitioned convolution using OLS modeled after ns_ols.h

   - code factored for matrix fir  (transform i/o once, compute matrix in FD)

   See pffft/LICENCE.txt

   Terminology:

   FD = Frequency Domain
   TD = Time Domain

   OLS = Overlap Save: how the circular convolutions (complex FD data
   multiplication) are used to implement linear convolution in FD,
   countering the wrap-around of the circular convolution by
   overlapping input and discarding wrap-around output.

   PC = Partitioned Convolution: how FIR is partitioned and input FD
   data is delayed to compute fast convolution of multiple sections
   using OLS

   Note that the OLS PC code has been moved into ns_pc.h and ns_pc_matrix_pc.h

*/




/* The default configuration (when MOD_PFFFT_CUSTOM is not defined) is
   built for a DSP processing block size of 256.

   - Input data is overlapped, using two consecutive blocks to provide
     the 512 sample input to the input FFT.

   - The transformed overlapped blocks are then stored in a delay line.

   - The FIR partitions are 256 samples paddeded with zeros.

*/


#ifndef MOD_PFFFT
#define MOD_PFFFT

// #define PFFFT_SIMD_DISABLE // debug


#include "macros.h"
#include "ilog.h"
#include "pffft_common.c"
#include "pffft.c"

/* These are hardcoded for the default use case which supports 256
   sample blocks OLS partitioned convolution. */
#ifndef  MOD_PFFFT_CUSTOM
#define  MOD_PFFFT_SIZE              512
#define  MOD_PFFFT_MAX_NB_PARTITIONS 5
#endif

#define MOD_PFFFT_PARTITION_SIZE (MOD_PFFFT_SIZE/2)
#define MOD_PFFFT_MAX_FIR_SIZE (MOD_PFFFT_MAX_NB_PARTITIONS * MOD_PFFFT_PARTITION_SIZE)

/* All float arrays need to be aligned to 4 float vector boundaries
   for the Neon implementation.  The algorithm will silently produce
   bad results if not aligned properly. */
#define MOD_PFFFT_ALIGN __attribute__((aligned(16)))

struct pffft_static {
    float data[MOD_PFFFT_SIZE];
    SETUP_STRUCT setup;
} MOD_PFFFT_ALIGN;

/* A buffer that can hold a real TD signal or a complex FD signal
   (half spectrum in pffft vector format) */
struct pffft_data {
    float data[MOD_PFFFT_SIZE];
} MOD_PFFFT_ALIGN;



/* The PFFFT_REAL transform produces only half of the complex
   spectrum, so 256 complex bins for 512 bytes real input size.  These
   are then grouped in 4 float vectors on ARM Neon */
#define  MOD_PFFFT_NCVEC         ((MOD_PFFFT_SIZE/2)/SIMD_SZ)


/* This replaces
   SETUP_STRUCT *FUNC_NEW_SETUP(int N, pffft_transform_t transform)
   from pffft/pffft_priv_impl.h
   with static allocation, i.e. a struct and an init function
   Only implemented for PFFFT_REAL
*/
void pffft_static_init(struct pffft_static *w) {
    SETUP_STRUCT *s = &w->setup;
    int N = MOD_PFFFT_SIZE;
    int k, m;

    s->N = N;
    s->transform = PFFFT_REAL;
    /* nb of complex simd vectors */
    s->Ncvec = MOD_PFFFT_NCVEC;
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












#endif
