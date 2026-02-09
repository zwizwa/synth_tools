/* Wrapper to include pffft code in the current compilation unit to
   avoid extra .o or .a files.  Also contains extra routines for
   static memory allocation.

   See pffft/LICENCE.txt

   Note that this is hardcoded for FFT size and using a REAL transform.

*/

#ifndef MOD_PFFFT
#define MOD_PFFFT

/* In the current use case these can be specialized for the
   compilation unit.

   FFT size 512 samples

   OLS chunks size (= tick size of DSP routine) is half of that: 256
   samples / block:
   - filter: 256 fir samples with 256 bytes of zero padding
   - input:  two consecutive 256 sample blocks concatenated

   The number of filter segments is 3.  This will need to be made a
   bit larger likely.

*/

#define  MOD_PFFFT_SIZE          512
#define  MOD_PFFFT_OLS_NB_BLOCKS 3
#define  MOD_PFFFT_TRANSFORM     PFFFT_REAL
#define  MOD_PFFFT_NCVEC         ((MOD_PFFFT_SIZE/2)/SIMD_SZ)

#include "macros.h"
#include "ilog.h"
#include "pffft_common.c"
#include "pffft.c"

/* This replaces
   SETUP_STRUCT *FUNC_NEW_SETUP(int N, pffft_transform_t transform)
   from pffft/pffft_priv_impl.h
   with static allocation, i.e. a struct and an init function
*/
struct uct_pf {
    v4sf data[2*MOD_PFFFT_NCVEC];
    SETUP_STRUCT setup;
};
void uct_pf_init(struct uct_pf *w) {
    SETUP_STRUCT *s = &w->setup;
    int N = MOD_PFFFT_SIZE;
    int k, m;

    s->N = N;
    s->transform = MOD_PFFFT_TRANSFORM;
    /* nb of complex simd vectors */
    s->Ncvec = MOD_PFFFT_NCVEC;
    s->data = &w->data[0];
    s->e = (float*)s->data;
    s->twiddle = (float*)(s->data + (2*s->Ncvec*(SIMD_SZ-1))/SIMD_SZ);

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

struct pffft_ols_block {
    v4sf freq[MOD_PFFFT_NCVEC];
};

struct pffft_ols {
    struct uct_pf wrap;
    struct pffft_ols_block filter[MOD_PFFFT_OLS_NB_BLOCKS];
    struct pffft_ols_block input[MOD_PFFFT_OLS_NB_BLOCKS];
    float work[MOD_PFFFT_SIZE*2];
    float overlap_in[MOD_PFFFT_SIZE];
    struct pffft_ols_block output;
    int next_block;
    struct ilog *ilog;
};

static inline void pffft_ols_init(struct pffft_ols *s,
                                  float *impulse,
                                  int nb_el) {
    //LOG("pffft_ols_init\n");

    // Clear the whole state.
    memset(s,0,sizeof(*s));

    uct_pf_init(&s->wrap);

    // Split the impulse in chunks and pre-compute FFT.
    int n = MOD_PFFFT_SIZE;
    float scale = 1.0f / ((float)n);

    int offset = 0;
    int chunk_size = n >> 1;
    for (int block = 0; block < MOD_PFFFT_OLS_NB_BLOCKS; block++) {
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
        pffft_transform(&s->wrap.setup,
                        ir_chunk_padded, ir_chunk_fft,
                        s->work,
                        PFFFT_FORWARD);
        //LOG("pffft_ols_init: post trans\n");
        offset += chunk_size;
    }
    //LOG("pffft_ols_init: done\n");

}

void vector_log_write_floats_fd(int fd, uint32_t cmd,
                                const float *vec, uint32_t len);



static inline void pffft_ols_tick(struct pffft_ols *s,
                                  const float *in,
                                  float *out) {
    LOG("pffft_ols_tick\n");
    ilog_floats(s->ilog, 0, in, 256);

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

    /* Compute the FFT of the overlapped input and place it in the FFT
       delay line in the correct slot. */
    int b_cur_input = s->next_block;
    s->next_block = (s->next_block + 1) % MOD_PFFFT_OLS_NB_BLOCKS;
    pffft_transform(&s->wrap.setup,
                    s->overlap_in, (float*)s->input[b_cur_input].freq,
                    s->work,
                    PFFFT_FORWARD);

    /* Perform frequency domain convolution for all the blocks in the
       FFT delay line.  The most recent input block (b_first) is
       circ-convolved with the first filter block (0). */

    // FIXME: Use the ab <- a*b method

    int nb = MOD_PFFFT_OLS_NB_BLOCKS; (void)nb;
    //LOG("fd:\n");
    float *o = (float*)s->output.freq;

    /* Current block. */
    LOG("pffft_ols_tick: convolve 0\n");
    pffft_zconvolve_no_accu(&s->wrap.setup,
                            (const float*)s->input[b_cur_input].freq,
                            (const float*)s->filter[0].freq,
                            o, 1.0f);
    /* Delayed blocks. */
    for (int b_filter=1; b_filter < MOD_PFFFT_OLS_NB_BLOCKS; b_filter++) {
        LOG("pffft_ols_tick: convolve %d\n", b_filter);
        int b_input = (nb + b_cur_input - b_filter) % nb;
        pffft_zconvolve_no_accu(&s->wrap.setup,
                                (float*)s->input[b_input].freq,
                                (float*)s->filter[b_filter].freq,
                                o, 1.0f);
    }

    /* Transform back. */
    float o_inv[2*n_div_2];

    pffft_transform(&s->wrap.setup,
                    o, o_inv,
                    s->work,
                    PFFFT_BACKWARD);

    //ilog_floats(s->ilog, 0, o_inv, ARRAY_SIZE(o_inv));

    /* Save the non-overlapping part of the output. */
    for (int k=0; k<n_div_2; k++) {
        //out[k] = o_inv[k + n_div_2];
    }

}


#endif
