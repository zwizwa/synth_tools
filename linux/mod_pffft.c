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

struct pffft_pc_worker {
    struct pffft_static fft;  // pffft state (coefficients, sizes)
    struct pffft_data work;   // temporary buffer for pffft functions
    struct ilog *ilog;        // optional ilog binary logger for logging float blocks
} MOD_PFFFT_ALIGN;

struct pffft_pc_fir {
    /* FIR partitions, TD zero-padded and transformed to FD. */
    struct pffft_data filter[MOD_PFFFT_MAX_NB_PARTITIONS];
    /* Number of partitions actuall populated. */
    uint32_t nb_partitions;
} MOD_PFFFT_ALIGN;

struct pffft_pc_input {
    /* Previous + current input block concatenated (TD) */
    struct pffft_data overlap_in;
    /* Input delay line, overlapped, transformed to FD */
    struct pffft_data input[MOD_PFFFT_MAX_NB_PARTITIONS];
    /* Location to write the next input block. */
    int next_block;
} MOD_PFFFT_ALIGN;

struct pffft_pc_output {
    struct pffft_data output;      // Output FD accumulator
    struct pffft_data overlap_out; // Transformed TD output accu
} MOD_PFFFT_ALIGN;

/* Bundled state to perform overlap save partitioned convolution on a
   single input, single output channel. */
struct pffft_pc {
    struct pffft_pc_worker worker;
    struct pffft_pc_fir    fir;
    struct pffft_pc_input  input;
    struct pffft_pc_output output;
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


static inline void pffft_pc_worker_init(struct pffft_pc_worker *w,
                                        struct ilog *ilog) {
    memset(w,0,sizeof(*w));
    w->ilog = ilog;
    pffft_static_init(&w->fft);
}

static inline void pffft_pc_fir_init(struct pffft_pc_worker *w,
                                     struct pffft_pc_fir *s,
                                     const float *impulse,
                                     int nb_el) {

    if (!impulse || !nb_el) {
        memset(s, 0, sizeof(*s));
        return;
    }

    // Split the impulse in chunks and pre-compute FFT.
    int n = MOD_PFFFT_SIZE;
    float scale = 1.0f / ((float)n);

    int offset = 0;
    int chunk_size = n >> 1;

    s->nb_partitions = 1 + (nb_el-1)/chunk_size;

    // LOG("nb_el = %d, nb_partitions = %d\n", nb_el, s->nb_partitions);

    ASSERT(s->nb_partitions <= MOD_PFFFT_MAX_NB_PARTITIONS);
    ASSERT(s->nb_partitions >= 1);

    for (int block = 0; block < s->nb_partitions; block++) {
        //LOG("block %d\n", block);
        float *ir_chunk_fft = (float*)s->filter[block].data;
        float ir_chunk_padded[n];
        memset(ir_chunk_padded, 0, sizeof(ir_chunk_padded));
        for (int i=0; i<chunk_size; i++) {
            int oi = offset + i;
            if (oi < nb_el) {
                //LOG("  offset %d\n", oi);
                ir_chunk_padded[i] = impulse[oi] * scale;
            }
        }

        //LOG("pffft_ols_init: pre trans\n");
        pffft_transform(&w->fft.setup,
                        ir_chunk_padded, ir_chunk_fft,
                        w->work.data,
                        PFFFT_FORWARD);
        //LOG("pffft_ols_init: post trans\n");

        //LOG("block %d, timei=%d, freqi=%d\n",
        //    block,
        //    (int)ilog_floats(s->ilog, 0, ir_chunk_padded, n),
        //    (int)ilog_floats(s->ilog, 0, ir_chunk_fft, n));

        offset += chunk_size;
    }
    //LOG("pffft_ols_init: done\n");

}

static inline void pffft_pc_input_init(struct pffft_pc_input *s) {
    memset(s,0,sizeof(*s));
}

static inline void pffft_pc_output_init(struct pffft_pc_output *s) {
    memset(s,0,sizeof(*s));
}

static inline void pffft_pc_init(struct pffft_pc *s,
                                 const float *impulse,
                                 int nb_el,
                                 struct ilog *ilog) {
    // The worker contains all the state necessary to run the
    // algorithm except for the input and FIR data.
    pffft_pc_worker_init(&s->worker, ilog);

    // FIR, input delay line and output accumulator are stored separately.
    pffft_pc_fir_init(&s->worker, &s->fir, impulse, nb_el);

    pffft_pc_input_init(&s->input);
    pffft_pc_output_init(&s->output);

}

static inline void pffft_pc_input_tick(struct pffft_pc_worker *w,
                                       struct pffft_pc_input *s,
                                       const float *in) {
    //ilog_floats(w->ilog, 0, in, 256);

    /* Overlap the input: keep a separate delay line for the real
       input.  Input/output block size is half of the FFT size
       NS(_logn), e.g. 256 float blocks means 512 point FFTs. */
    int n = MOD_PFFFT_SIZE;
    int n_div_2 = n/2;

    for(int k=0; k<n_div_2; k++) {
        float *old = &s->overlap_in.data[k];
        float *new = &s->overlap_in.data[k + n_div_2];
        /* Shift previous input into left (older) slot. */
        *old = *new;
        /* Copy new nput into right (newer) slot. */
        *new = in[k];
    }

    /* The number of partitions to compute is always determined by the
       FIR.  The input might have more delay partitions to accomodate
       other, longer FIRs, but not less. */

    /* Compute the FFT of the overlapped input and place it in the FFT
       delay line in the correct slot.  Note that we just use MAX_NB
       here to keep the implementation of the matrix FIR simpler,
       i.e. make it more uniform. */
    int b_cur_input = s->next_block;
    s->next_block = (s->next_block + 1) % MOD_PFFFT_MAX_NB_PARTITIONS;
    pffft_transform(&w->fft.setup,
                    s->overlap_in.data,
                    s->input[b_cur_input].data,
                    w->work.data,
                    PFFFT_FORWARD);

}

static inline const float *pffft_pc_input_partition(const struct pffft_pc_input *in,
                                                    int b_filter) {
    int inbp = MOD_PFFFT_MAX_NB_PARTITIONS;
    int b_cur_input = in->next_block - 1;
    int b_input = (inbp*2 + b_cur_input - b_filter) % inbp;
    return in->input[b_input].data;
}

static inline void pffft_pc_convolve_tick(struct pffft_pc_worker *w,
                                          const struct pffft_pc_input *in,
                                          const struct pffft_pc_fir *fir,
                                          struct pffft_pc_output *out,
                                          int accumulate) {

    /* Perform frequency domain convolution for all the blocks in the
       FFT delay line.  The most recent input block (b_first) is
       circ-convolved with the first filter block (0). */

    float *o = out->output.data;

    /* Current block. */
    (accumulate ? pffft_zconvolve_accumulate : pffft_zconvolve_no_accu)(
        &w->fft.setup,
        pffft_pc_input_partition(in, 0),
        fir->filter[0].data,
        o, 1.0f);

    //LOG("pffft_ols_tick: convolve 0 done, li=%d\n",
    //    (int)ilog_floats(s->ilog, 0, o, MOD_PFFFT_SIZE);

    /* Delayed blocks. */
    for (int b_filter=1; b_filter < fir->nb_partitions; b_filter++) {
        // FIXME: split off in inline function
        pffft_zconvolve_accumulate(
            &w->fft.setup,
            pffft_pc_input_partition(in, b_filter),
            fir->filter[b_filter].data,
            o, 1.0f);
        //LOG("pffft_ols_tick: convolve %d done\n", b_filter);
        //ilog_floats(s->ilog, 0, o, MOD_PFFFT_SIZE );
    }

}

static inline void pffft_pc_output_tick(struct pffft_pc_worker *w,
                                        struct pffft_pc_output *out_accu,
                                        float *out) {
    float *o = out_accu->output.data;

    /* Transform back. */
    pffft_transform(&w->fft.setup,
                    o, out_accu->overlap_out.data,
                    w->work.data,
                    PFFFT_BACKWARD);

    //LOG("overlap_out=%d\n",
    //    (int)ilog_floats(s->ilog, 0, s->overlap_out, n));


    //ilog_floats(s->ilog, 0, s->overlap_out, n);

    /* Save the non-overlapping part of the output. */
    //LOG("pffft_ols_tick: pre out\n");

    int n = MOD_PFFFT_SIZE;
    int n_div_2 = n/2;

    for (int k=0; k<n_div_2; k++) {
        out[k] = out_accu->overlap_out.data[k + n_div_2];
    }

    //LOG("out=%d\n",
    //    (int)ilog_floats(s->ilog, 0, out, n/2));

}


static inline void pffft_pc_tick(struct pffft_pc *s,
                                 const float *in,
                                 float *out) {

    /* Push a new block into the input delay line. */
    pffft_pc_input_tick(&s->worker, &s->input, in);

    /* FD convolve into FD accumulator. */
    pffft_pc_convolve_tick(&s->worker,
                           &s->input,
                           &s->fir,
                           &s->output,
                           0 /* overwrite */);

    /* Transform output FD->TD and copy the output data, ignoring the
       wrap-around from circular convolution. */
    pffft_pc_output_tick(&s->worker, &s->output, out);

}


/* Note: Matrix filter.

   For multi-channel filtering when there is input fan-out and output
   summing, the fanout and summing can be done in the frequency domain
   meaning there is only one FFT per input, one IFFT per output, and
   an NI x NO matrix of FIR filters computed in the frequency domain
   using partitioned convolution.

   Left as an exercise for the reader ;-)

*/



/* TODO: numeric properties

   It might be better to sort the sections by power spectrum and
   accumulate them from small to large.

*/


#endif
