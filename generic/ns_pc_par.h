/* A collection of independent filters with associated input delay
   lines that can reuse the same worker state.  Note that the
   NS(_pc_output) struct is temporary so this doesn't work for
   parallel thread computation. */
struct NS(_pc_par_state) {
    struct NS(_pc_worker) worker;
    struct NS(_pc_fir)    fir[NS(_pc_par_nb)];
    struct NS(_pc_input)  input[NS(_pc_par_nb)];
    struct NS(_pc_output) output;
} MOD_PFFFT_ALIGN;

static inline void NS(_pc_par_fir_init)(struct NS(_pc_par_state) *s,
                                        int c,  // channel
                                        const float *impulse,
                                        int nb_el) {
    NS(_pc_fir_init)(&s->worker, &s->fir[c], impulse,nb_el, 1);
}



static inline void NS(_pc_par_init)(struct NS(_pc_par_state) *s,
                                    struct ilog *ilog) {

    // The worker contains all the state necessary to run the
    // algorithm except for the input and FIR data.
    NS(_pc_worker_init)(&s->worker, ilog);

    // The firs still need to be initialized individually.  Start out
    // with zero = mute channel.
    memset(&s->fir, 0, sizeof(s->fir));

    for (int c=0; c<NS(_pc_par_nb); c++) {
        NS(_pc_input_init)(&s->input[c]);
    }
    NS(_pc_output_init)(&s->output);
}

static inline void NS(_pc_par_tick)(struct NS(_pc_par_state) *s,
                                    float * const* in,
                                    float **out) {

    /* Compute the spectral multiplication for the FIR matrix. */
    for (int c=0; c<NS(_pc_par_nb); c++) {

        /* Push new block into the input delay line. */
        NS(_pc_input_tick)(&s->worker, &s->input[c], in[c]);

        /* FD convolve into FD accumulator. */
        NS(_pc_convolve_tick)(&s->worker,
                              &s->input[c],
                              &s->fir[c],
                              &s->output,
                              0 /* initialize, don't accumulate */);

        /* Transform output FD->TD and copy the output data, ignoring the
           wrap-around from circular convolution. */
        NS(_pc_output_tick)(&s->worker, &s->output, out[c]);
    }
}

