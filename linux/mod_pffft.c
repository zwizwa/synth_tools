/* This module wraps the pffft library from
   https://github.com/marton78/pffft
   d321d006467fcdcafc4298901c27541a18da598c

   Together with some ns*.h NS modules it provides:

   - replacement initialization function and struct for static data allocation

   - direct inclusion of source files to avoid extra .o .a and provide
     more optimalization opportunity

   - delay line for partitioned convolution using OLS modeled after ns_ols.h

   - code factored for single channel and matrix fir (transform i/o
     once, compute matrix in FD)

   For partent project license see pffft/LICENCE.txt

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

   All static code is written as NS modules
   - ns_pffft.h          static pffft wrapper
   - ns_pc.h             single channel fir
   - ns_pc_matrix_pc.h   matrix fir

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

// #define PFFFT_SIMD_DISABLE // for debug
#include "macros.h"
#include "ilog.h"
#include "pffft_common.c"
#include "pffft.c"

/* All float arrays need to be aligned to 4 float vector boundaries
   for the Neon implementation.  The algorithm will silently produce
   bad results if not aligned properly. */
#define MOD_PFFFT_ALIGN_BYTES 16
#define MOD_PFFFT_ALIGN  __attribute__((aligned(MOD_PFFFT_ALIGN_BYTES)))
static inline void assert_pffft_align(const void *ptr) {
    if (1) {
        uintptr_t addr = (uintptr_t)ptr;
        uintptr_t align_error = addr % MOD_PFFFT_ALIGN_BYTES;
        if (unlikely(align_error)) {
            fprintf(stderr, "bad pffft align %p\n", ptr);
        }
    }
}

/* Wrap the pffft_transform call */
static inline void mod_pffft_transform(PFFFT_Setup *setup, const float *input, float *output, float *work, pffft_direction_t direction) {
    // fprintf(stderr, "%p %p %p\n", input, output, work);
    assert_pffft_align(input);
    assert_pffft_align(output);
    assert_pffft_align(work);
    pffft_transform(setup, input, output, work, direction);
}

static inline void mod_pffft_transform_ordered(PFFFT_Setup *setup, const float *input, float *output, float *work, pffft_direction_t direction) {
    assert_pffft_align(input);
    assert_pffft_align(output);
    assert_pffft_align(work);
    pffft_transform_ordered(setup, input, output, work, direction);
}


/* Default parameterizations. */
#define NS(name) pffft_r512##name
#define pffft_r512_fft_size           512
#define pffft_r512_max_nb_partitions  5
#include "ns_pffft.h"
#undef NS

#define NS(name) pffft_r256##name
#define pffft_r256_fft_size           256
#define pffft_r256_max_nb_partitions  10
#include "ns_pffft.h"
#undef NS

//#define MOD_PFFFT_MAX_FIR_SIZE (MOD_PFFFT_MAX_NB_PARTITIONS * MOD_PFFFT_PARTITION_SIZE)

#endif
