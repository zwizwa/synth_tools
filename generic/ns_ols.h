struct NS(_ols_block) {
    NS(_data_t) freq[1<<NS(_logn)];
};
struct NS(_ols_state) {
    struct NS(_ctx) fft_ctx;
    struct NS(_ols_block) input[NS(_ols_nb_blocks)];
    struct NS(_ols_block) filter[NS(_ols_nb_blocks)];
    struct NS(_ols_block) output;
    NS(_real_t) overlap_in[1<<NS(_logn)];
    int next_block;
};

static inline void NS(_ols)(struct NS(_ols_state) *s,
                            const NS(_real_t) *in,
                            NS(_real_t) *out) {

    /* Overlap the input: keep a separate delay line for the real
       input.  Input/output block size is half of the FFT size
       NS(_logn), e.g. 256 float blocks means 512 point FFTs. */
    int n_div_2 = 1<<(NS(_logn)-1);
    for(int k=0; k<n_div_2; k++) {
        NS(_real_t) *old = &s->overlap_in[k];
        NS(_real_t) *new = &s->overlap_in[k + n_div_2];
        /* Shift previous input into left (older) slot. */
        *old = *new;
        /* Copy new nput into right (newer) slot. */
        *new = in[k];
    }

    /* Compute the FFT of the overlapped input and place it in the FFT
       delay line in the correct slot. */
    int b_first = s->next_block;
    s->next_block = (s->next_block + 1) % NS(_ols_nb_blocks);
    NS(_process_real)(&s->fft_ctx, s->overlap_in, s->input[b_first].freq);

    /* Perform frequency domain convolution for all the blocks in the
       FFT delay line. */
    NS(_data_t) *o = s->output.freq;
    for (int b=0; b<1; b++) {
        int b_offset = (b + b_first) % NS(_ols_nb_blocks);

        NS(_data_t) *i = s->input[b_offset].freq;
        NS(_data_t) *f = s->filter[b].freq;
        for (int n=0; n<(1<<(NS(_logn))); n++) {
            NS(_data_mul3)(&o[n], &i[n], &f[n]); // o = i * f  (b==0)
        }
    }
    for (int b=1; b<NS(_ols_nb_blocks); b++) {
        int b_offset = (b + b_first) % NS(_ols_nb_blocks);

        NS(_data_t) *i = s->input[b_offset].freq;
        NS(_data_t) *f = s->filter[b].freq;
        for (int n=0; n<(1<<(NS(_logn))); n++) {
            NS(_data_mac3)(&o[n], &i[n], &f[n]); // o += i * f  (b>0)
        }
    }

    /* Save the non-overlapping part of the output. */

}

/* The basic step of the OLS method is to perform circular convolution:

   - [ dI I ]   last block, current block
   - [ <  0 ]   impulse, padded
   - [ O  _ ]   last block is output. the ignored part contains overlapped data
*/

static inline void NS(_ols_init)(struct NS(_ols_state) *s,
                                 const NS(_real_t) *impulse,
                                 int nb_el) {
    for (int block = 0; block < NS(_ols_nb_blocks); block++) {
        NS(_data_t) *b = s->filter[block].freq;
        /* FIXME: Do this incrementally to get the coordinates
           right. */
        (void)b;

    }

}
