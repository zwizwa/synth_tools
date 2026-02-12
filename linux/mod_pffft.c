/* This module wraps the pffft library from
   https://github.com/marton78/pffft
   d321d006467fcdcafc4298901c27541a18da598c
   And provides:

   - replacement initialization function and struct for static data allocation

   - direct inclusion of source files to avoid extra .o .a and provide
     more optimalization opportunity

   - delay line for partitioned convolution using OLS modeled after ns_ols.h

   See pffft/LICENCE.txt

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

// #define PFFFT_SIMD_DISABLE // For testing


#include "macros.h"
#include "ilog.h"
#include "pffft_common.c"
#include "pffft.c"

/* See mod_pffft.c for implementation.

   Note that this header contains optional compile-time configuration
   so maybe better to wrap it in another header that includes the
   project-specific configuration to make sure the header and
   implementation do not get out of sync.
*/

/* These are hardcoded for the default use case which supports 256
   sample blocks OLS partitioned convolution. */
#ifndef  MOD_PFFFT_CUSTOM
#define  MOD_PFFFT_SIZE          512
#define  MOD_PFFFT_NB_PARTITIONS 3
#define  MOD_PFFFT_TRANSFORM     PFFFT_REAL
#endif

/* All float arrays need to be aligned to 4 float vector boundaries
   for the Neon implementation.  The algorithm will silently produce
   bad results if not aligned properly. */
#define MOD_PFFFT_ALIGN __attribute__((aligned(16)))

struct pffft_static {
    float data[MOD_PFFFT_SIZE];
    // SETUP_STRUCT setup;
    PFFFT_Setup setup;
} MOD_PFFFT_ALIGN;

struct pffft_partition {
    float freq[MOD_PFFFT_SIZE];
} MOD_PFFFT_ALIGN;

struct pffft_pc_worker {
    /* Worker state. */
    struct pffft_static fft;              // pffft state (coefficients)
    float work[MOD_PFFFT_SIZE];
    struct ilog *ilog;                    // optional ilog binary logger for logging float blocks
} MOD_PFFFT_ALIGN;

struct pffft_pc_fir {
    /* FIR coefficients. */
    struct pffft_partition filter[MOD_PFFFT_NB_PARTITIONS];  // FIR partitions, zero-padded (FD)
    uint32_t nb_partitions;                                  // number of FIR partitions actually populated
} MOD_PFFFT_ALIGN;

struct pffft_pc_input {
    /* Input delay line. */
    float overlap_in[MOD_PFFFT_SIZE];                        // Previous + current input block concatenated (TD)
    struct pffft_partition input[MOD_PFFFT_NB_PARTITIONS];   // Input delay line, overlapped (FD)
    int next_block;                                          // next index into the input (FD) delay line
    uint32_t nb_partitions;                                  // number of FIR partitions actually populated
} MOD_PFFFT_ALIGN;

struct pffft_pc_output {
    struct pffft_partition output;        // Output FD accumulator
    float overlap_out[MOD_PFFFT_SIZE];    // Transformed TD output accu
} MOD_PFFFT_ALIGN;



// FD = Frequency Domain
// TD = Time Domain
struct pffft_pc { // overlap save partitioned convolution
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
*/


void pffft_static_init(struct pffft_static *w) {
    SETUP_STRUCT *s = &w->setup;
    int N = MOD_PFFFT_SIZE;
    int k, m;

    s->N = N;
    s->transform = MOD_PFFFT_TRANSFORM;
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


/* Re-implementation of ns_ols.h to make use of pffft routines.

   It doesn't make sense to try to fit this into ns_ols.h due to
   different assumptions.  The routines are not that complicated to
   just re-implement and then validate in a quickcheck test.
*/

static inline void pffft_pc_worker_init(struct pffft_pc_worker *w,
                                        struct ilog *ilog) {
    memset(w,0,sizeof(*w));
    w->ilog = ilog;
    pffft_static_init(&w->fft);
}

static inline int pffft_pc_fir_init(struct pffft_pc_worker *w,
                                    struct pffft_pc_fir *s,
                                    float *impulse,
                                    int nb_el) {

    if (!impulse) return 0;
    if (nb_el == 0) return 0;

    // Split the impulse in chunks and pre-compute FFT.
    int n = MOD_PFFFT_SIZE;
    float scale = 1.0f / ((float)n);

    int offset = 0;
    int chunk_size = n >> 1;

    s->nb_partitions = 1 + (nb_el-1)/chunk_size;

    // LOG("nb_el = %d, nb_partitions = %d\n", nb_el, s->nb_partitions);

    ASSERT(s->nb_partitions <= MOD_PFFFT_NB_PARTITIONS);
    ASSERT(s->nb_partitions >= 1);

    for (int block = 0; block < s->nb_partitions; block++) {
        //LOG("block %d\n", block);
        float *ir_chunk_fft = (float*)s->filter[block].freq;
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
                        w->work,
                        PFFFT_FORWARD);
        //LOG("pffft_ols_init: post trans\n");

        //LOG("block %d, timei=%d, freqi=%d\n",
        //    block,
        //    (int)ilog_floats(s->ilog, 0, ir_chunk_padded, n),
        //    (int)ilog_floats(s->ilog, 0, ir_chunk_fft, n));

        offset += chunk_size;
    }
    //LOG("pffft_ols_init: done\n");

    return s->nb_partitions;
    
}

static inline void pffft_pc_input_init(struct pffft_pc_input *s,
                                       int nb_partitions) {
    memset(s,0,sizeof(*s));
    s->nb_partitions = nb_partitions;
}

static inline void pffft_pc_output_init(struct pffft_pc_output *s) {
    memset(s,0,sizeof(*s));
}

static inline void pffft_pc_init(struct pffft_pc *s,
                                     float *impulse,
                                     int nb_el,
                                     struct ilog *ilog) {
    // Clear the whole state.
    memset(s,0,sizeof(*s));

    // The worker contains all the state necessary to run the
    // algorithm except for the input and FIR data.
    pffft_pc_worker_init(&s->worker, ilog);

    // FIR, input delay line and output accumulator are stored separately.
    int nbp = pffft_pc_fir_init(&s->worker, &s->fir, impulse, nb_el);
    ASSERT(nbp <= MOD_PFFFT_NB_PARTITIONS);

    pffft_pc_input_init(&s->input, nbp);
    pffft_pc_output_init(&s->output);

}

static inline int pffft_pc_input_tick(struct pffft_pc_worker *w,
                                      struct pffft_pc_input *s,
                                      const float *in) {
    //LOG("pffft_ols_tick\n");
    //ilog_floats(w->ilog, 0, in, 256);

    /* Overlap the input: keep a separate delay line for the real
       input.  Input/output block size is half of the FFT size
       NS(_logn), e.g. 256 float blocks means 512 point FFTs. */
    int n = MOD_PFFFT_SIZE;
    int n_div_2 = n/2;

    for(int k=0; k<n_div_2; k++) {
        float *old = &s->overlap_in[k];
        float *new = &s->overlap_in[k + n_div_2];
        /* Shift previous input into left (older) slot. */
        *old = *new;
        /* Copy new nput into right (newer) slot. */
        *new = in[k];
    }

    /* The number of partitions to compute is always determined by the
       FIR.  The input might have more delay partitions to accomodate
       other, longer FIRs, but not less. */

    /* Compute the FFT of the overlapped input and place it in the FFT
       delay line in the correct slot. */
    int b_cur_input = s->next_block;
    s->next_block = (s->next_block + 1) % s->nb_partitions;
    pffft_transform(&w->fft.setup,
                    s->overlap_in,
                    s->input[b_cur_input].freq,
                    w->work,
                    PFFFT_FORWARD);

    return b_cur_input;

}

#if 0
static inline float *pffft_pc_input_partition(struct pffft_pc_input *in,
                                              int b_filter) {
    int inbp = in->nb_partitions;
    int b_cur_inptut = in->next_block - 1;
    int b_input = (inbp*2 + b_cur_input - b_filter) % inbp;
}
#endif

static inline void pffft_pc_convolve_tick(struct pffft_pc_worker *w,
                                          const struct pffft_pc_input *in,
                                          const struct pffft_pc_fir *fir,
                                          struct pffft_pc_output *out,
                                          int b_cur_input) {

    /* Perform frequency domain convolution for all the blocks in the
       FFT delay line.  The most recent input block (b_first) is
       circ-convolved with the first filter block (0). */

    // FIXME: Use the ab <- a*b method

    //LOG("fd:\n");
    float *o = out->output.freq;

    /* Current block. */
    pffft_zconvolve_no_accu(&w->fft.setup,
                            in->input[b_cur_input].freq,
                            fir->filter[0].freq,
                            o, 1.0f);
    //LOG("pffft_ols_tick: convolve 0 done, li=%d\n",
    //    (int)ilog_floats(s->ilog, 0, o, n);

    /* Delayed blocks. */
    for (int b_filter=1; b_filter < fir->nb_partitions; b_filter++) {
        // FIXME: split off in inline function
        int inbp = in->nb_partitions;
        int b_input = (inbp + b_cur_input - b_filter) % inbp;
        pffft_zconvolve_accumulate(&w->fft.setup,
                                   in->input[b_input].freq,
                                   fir->filter[b_filter].freq,
                                   o, 1.0f);
        //LOG("pffft_ols_tick: convolve %d done\n", b_filter);
        //ilog_floats(s->ilog, 0, o, n);
    }

}

static inline void pffft_pc_output_tick(struct pffft_pc_worker *w,
                                        struct pffft_pc_output *out_accu,
                                        float *out) {
    //LOG("fd:\n");
    float *o = out_accu->output.freq;

    /* Transform back. */
    pffft_transform(&w->fft.setup,
                    o, out_accu->overlap_out,
                    w->work,
                    PFFFT_BACKWARD);

    //LOG("overlap_out=%d\n",
    //    (int)ilog_floats(s->ilog, 0, s->overlap_out, n));


    //ilog_floats(s->ilog, 0, s->overlap_out, n);

    /* Save the non-overlapping part of the output. */
    //LOG("pffft_ols_tick: pre out\n");

    int n = MOD_PFFFT_SIZE;
    int n_div_2 = n/2;

    for (int k=0; k<n_div_2; k++) {
        out[k] = out_accu->overlap_out[k + n_div_2];
    }

    //LOG("out=%d\n",
    //    (int)ilog_floats(s->ilog, 0, out, n/2));

}


static inline void pffft_pc_tick(struct pffft_pc *s,
                                 const float *in,
                                 float *out) {

    /* Make sure the input and FIR are compatible. */
    ASSERT(s->input.nb_partitions >= s->fir.nb_partitions);

    /* Push a new block into the input delay line. */
    int b_cur_input = pffft_pc_input_tick(&s->worker, &s->input, in);

    /* Convolve into FD accumulator. */
    // FIXME: separate out the output
    pffft_pc_convolve_tick(&s->worker,
                           &s->input,
                           &s->fir,
                           &s->output,
                           b_cur_input);

    pffft_pc_output_tick(&s->worker, &s->output, out);

}


/* Matrix filter.

   For multi-channel filtering when there is input fan-out and output
   mixing, the fanout and summing can be done in the frequency domain
   meaning there is only one FFT per input, one IFFT per output, and
   an NI x NO matrix of FIR filters computed in the frequency domain
   using partitioned convolution.

*/

// TODO

// - split nb_partitions in delay line and filter, since these will be
//   separate, e.g. it is possible that an input has delays for 4
//   partitions to support a 4-partition FIR, but another filter using
//   the same input could have less partitions.
//
// - there are actually 3 parts of state:
//   - fft setup
//   - fir
//   - delay line
//
//   The fft setup is actually the state of the "worker" that performs
//   the FFT.  Can this be split up already?



#endif
