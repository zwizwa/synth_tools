/* Matrix filter.

   For multi-channel filtering when there is input fan-out and output
   summing, the fanout and summing can be done in the frequency domain
   meaning there is only one FFT per input, one IFFT per output, and
   an NI x NO matrix of FIR filters computed in the frequency domain
   using partitioned convolution.
*/


struct NS(_pc_matrix_state) {
    struct NS(_pc_worker) worker;
    struct NS(_pc_fir)    fir[NS(_pc_matrix_nb_in)][NS(_pc_matrix_nb_out)];
    struct NS(_pc_input)  input[NS(_pc_matrix_nb_in)];
    struct NS(_pc_output) output[NS(_pc_matrix_nb_out)];
} MOD_PFFFT_ALIGN;

static inline void NS(_pc_matrix_fir_init)(struct NS(_pc_matrix_state) *s,
                                           int i,  // input index
                                           int o,  // output index
                                           const float *impulse,
                                           int nb_el) {
    NS(_pc_fir_init)(&s->worker, &s->fir[i][o], impulse,nb_el, 1);
}



static inline void NS(_pc_matrix_init)(struct NS(_pc_matrix_state) *s,
                                       struct ilog *ilog) {

    // The worker contains all the state necessary to run the
    // algorithm except for the input and FIR data.
    NS(_pc_worker_init)(&s->worker, ilog);

    // Note that the individual filters need to be initialized.  They
    // are set to mute by default.
    memset(&s->fir, 0, sizeof(s->fir));

    for (int i=0; i<NS(_pc_matrix_nb_in); i++) {
        NS(_pc_input_init)(&s->input[i]);
    }
    for (int o=0; o<NS(_pc_matrix_nb_out); o++) {
        NS(_pc_output_init)(&s->output[o]);
    }

}

static inline void NS(_pc_matrix_tick)(struct NS(_pc_matrix_state) *s,
                                       float * const* in,
                                       float **out) {

    /* Push new blocks into the input delay lines. */
    for (int i=0; i<NS(_pc_matrix_nb_in); i++) {
        NS(_pc_input_tick)(&s->worker, &s->input[i], in[i]);
    }

    /* Compute the spectral multiplication for the FIR matrix. */
    for (int o=0; o<NS(_pc_matrix_nb_out); o++) {
        for (int i=0; i<NS(_pc_matrix_nb_in); i++) {

            /* FD convolve into FD accumulator. */
            int accumulate = !(i == 0);
            NS(_pc_convolve_tick)(&s->worker,
                                  &s->input[i],
                                  &s->fir[i][o],
                                  &s->output[o],
                                  accumulate);
        }

        /* Transform output FD->TD and copy the output data, ignoring the
           wrap-around from circular convolution. */
        NS(_pc_output_tick)(&s->worker, &s->output[o], out[o]);
    }
}

