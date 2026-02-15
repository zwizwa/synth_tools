// FIXME: This still relies on pfft_ wrappers in mod_pfft.c, which should also be parameterized.
// It is in principle not dependent on the FFT implementation, just the FFT size used in the structs.

struct NS(_pc_worker) {
    struct pffft_static fft;  // pffft state (coefficients, sizes)
    struct pffft_data work;   // temporary buffer for pffft functions
    struct ilog *ilog;        // optional ilog binary logger for logging float blocks
} MOD_PFFFT_ALIGN;

struct NS(_pc_fir) {
    /* FIR partitions, TD zero-padded and transformed to FD. */
    struct pffft_data filter[MOD_PFFFT_MAX_NB_PARTITIONS];
    /* Number of partitions actuall populated. */
    uint32_t nb_partitions;
} MOD_PFFFT_ALIGN;

struct NS(_pc_input) {
    /* Previous + current input block concatenated (TD) */
    struct pffft_data overlap_in;
    /* Input delay line, overlapped, transformed to FD */
    struct pffft_data input[MOD_PFFFT_MAX_NB_PARTITIONS];
    /* Location to write the next input block. */
    int next_block;
} MOD_PFFFT_ALIGN;

struct NS(_pc_output) {
    struct pffft_data output;      // Output FD accumulator
    struct pffft_data overlap_out; // Transformed TD output accu
} MOD_PFFFT_ALIGN;

/* Bundled state to perform overlap save partitioned convolution on a
   single input, single output channel. */
struct NS(_pc_state) {
    struct NS(_pc_worker) worker;
    struct NS(_pc_fir)    fir;
    struct NS(_pc_input)  input;
    struct NS(_pc_output) output;
} MOD_PFFFT_ALIGN;


static inline void NS(_pc_worker_init)(struct NS(_pc_worker) *w,
                                       struct ilog *ilog) {
    memset(w,0,sizeof(*w));
    w->ilog = ilog;
    pffft_static_init(&w->fft);
}

static inline void NS(_pc_fir_init)(struct NS(_pc_worker) *w,
                                    struct NS(_pc_fir) *s,
                                    const float *impulse,
                                    int nb_el,
                                    int stride) {

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
                ir_chunk_padded[i] = impulse[oi * stride] * scale;
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

static inline void NS(_pc_input_init)(struct NS(_pc_input) *s) {
    memset(s,0,sizeof(*s));
}

static inline void NS(_pc_output_init)(struct NS(_pc_output) *s) {
    memset(s,0,sizeof(*s));
}

static inline void NS(_pc_init)(struct NS(_pc_state) *s,
                                const float *impulse,
                                int nb_el,
                                struct ilog *ilog) {
    // The worker contains all the state necessary to run the
    // algorithm except for the input and FIR data.
    NS(_pc_worker_init)(&s->worker, ilog);

    // FIR, input delay line and output accumulator are stored separately.
    NS(_pc_fir_init)(&s->worker, &s->fir, impulse, nb_el, 1);

    NS(_pc_input_init)(&s->input);
    NS(_pc_output_init)(&s->output);

}

static inline void NS(_pc_input_tick)(struct NS(_pc_worker) *w,
                                      struct NS(_pc_input) *s,
                                      const float *in) {
    //ilog_floats(w->ilog, 0, in, 256);

    /* Overlap the input: keep a separate delay line for the real
       input.  Input/output block size is half of the FFT size
       NS(_pc_logn), e.g. 256 float blocks means 512 point FFTs. */
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





static inline const float *NS(_pc_input_partition)(const struct NS(_pc_input) *in,
                                               int b_filter) {
    int inbp = MOD_PFFFT_MAX_NB_PARTITIONS;
    int b_cur_input = in->next_block - 1;
    int b_input = (inbp*2 + b_cur_input - b_filter) % inbp;
    return in->input[b_input].data;
}


static inline void NS(_pc_convolve_tick)(struct NS(_pc_worker) *w,
                                         const struct NS(_pc_input) *in,
                                         const struct NS(_pc_fir) *fir,
                                         struct NS(_pc_output) *out,
                                         int accumulate) {

    /* Perform frequency domain convolution for all the blocks in the
       FFT delay line.  The most recent input block (b_first) is
       circ-convolved with the first filter block (0). */

    float *o = out->output.data;

    /* Current block. */
    (accumulate ? pffft_zconvolve_accumulate : pffft_zconvolve_no_accu)(
        &w->fft.setup,
        NS(_pc_input_partition)(in, 0),
        fir->filter[0].data,
        o, 1.0f);

    //LOG("pffft_ols_tick: convolve 0 done, li=%d\n",
    //    (int)ilog_floats(s->ilog, 0, o, MOD_PFFFT_SIZE);

    /* Delayed blocks. */
    for (int b_filter=1; b_filter < fir->nb_partitions; b_filter++) {
        // FIXME: split off in inline function
        pffft_zconvolve_accumulate(
            &w->fft.setup,
            NS(_pc_input_partition)(in, b_filter),
            fir->filter[b_filter].data,
            o, 1.0f);
        //LOG("pffft_ols_tick: convolve %d done\n", b_filter);
        //ilog_floats(s->ilog, 0, o, MOD_PFFFT_SIZE );
    }

}
static inline void NS(_pc_output_tick)(struct NS(_pc_worker) *w,
                                       struct NS(_pc_output) *out_accu,
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


static inline void NS(_pc_tick)(struct NS(_pc_state) *s,
                                const float *in,
                                float *out) {

    /* Push a new block into the input delay line. */
    NS(_pc_input_tick)(&s->worker, &s->input, in);

    /* FD convolve into FD accumulator. */
    NS(_pc_convolve_tick)(&s->worker,
                       &s->input,
                       &s->fir,
                       &s->output,
                       0 /* overwrite */);

    /* Transform output FD->TD and copy the output data, ignoring the
       wrap-around from circular convolution. */
    NS(_pc_output_tick)(&s->worker, &s->output, out);

}



/* TODO: numeric properties

   It might be better to sort the sections by power spectrum and
   accumulate them from small to large.

*/
