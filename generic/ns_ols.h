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

    // LOG("_ols\n");

    /* Overlap the input: keep a separate delay line for the real
       input.  Input/output block size is half of the FFT size
       NS(_logn), e.g. 256 float blocks means 512 point FFTs. */
    int n_div_2 = 1 << (NS(_logn)-1);
    int n = n_div_2 << 1;

    for(int k=0; k<n_div_2; k++) {
        NS(_real_t) *old = &s->overlap_in[k];
        NS(_real_t) *new = &s->overlap_in[k + n_div_2];
        /* Shift previous input into left (older) slot. */
        *old = *new;
        /* Copy new nput into right (newer) slot. */
        *new = in[k];
    }

    /* Print the input time domain signal. */
    // LOG("overlap_in:");
    // NS(_log_real_vec)(s->overlap_in, n);

    /* Compute the FFT of the overlapped input and place it in the FFT
       delay line in the correct slot. */
    int b_first = s->next_block;
    s->next_block = (s->next_block + 1) % NS(_ols_nb_blocks);
    NS(_dir_fwd)(&s->fft_ctx);
    NS(_process_real)(&s->fft_ctx, s->overlap_in, s->input[b_first].freq);

    /* Print it */
    // LOG("fft of overlap_in %d: ", b_first);
    // NS(_log_data_vec)(s->input[b_first].freq, n);

    /* Perform frequency domain convolution for all the blocks in the
       FFT delay line. */
    int nb = NS(_ols_nb_blocks);
    NS(_data_t) *o = s->output.freq;
    {
        int b = 0;
        int b_offset = b_first;
        // LOG("input %d x filter %d\n", b_offset, b);

        NS(_data_t) *i = s->input[b_offset].freq;
        NS(_data_t) *f = s->filter[b].freq;
        for (int k=0; k<n; k++) {
            NS(_data_mul3)(&o[k], &i[k], &f[k]); // o = i * f  (b==0)
        }
    }
    for (int b=1; b<NS(_ols_nb_blocks); b++) {
        int b_offset = (nb + b_first - b) % nb;
        // LOG("input %d x filter %d\n", b_offset, b);

        NS(_data_t) *i = s->input[b_offset].freq;
        NS(_data_t) *f = s->filter[b].freq;
        for (int k=0; k<n; k++) {
            NS(_data_mac3)(&o[k], &i[k], &f[k]); // o += i * f  (b>0)
        }
    }

    /* Transform back. */
    NS(_data_t) o_inv[2*n_div_2];
    NS(_dir_rev)(&s->fft_ctx);
    NS(_process)(&s->fft_ctx, o, o_inv);

#if 0
    for (int k=0; k<n; k++) {
        LOG(" %d", o_inv[k]);
    }
    LOG(" <-o_inv\n");
#endif

    /* Save the non-overlapping part of the output. */
    for (int k=0; k<n_div_2; k++) {
        NS(_to_real)(&out[k], &o_inv[k + n_div_2]);
    }

}

/* The basic step of the OLS method is to perform circular convolution
 * with the following vector layout:

   - [ dI I ]   prev block, current block
   - [ <  0 ]   impulse, padded
   - [ O  _ ]   last block is output. the ignored part contains overlapped data
*/

static inline void NS(_ols_init)(struct NS(_ols_state) *s,
                                 const NS(_real_t) *impulse,
                                 int nb_el) {
    NS(_dir_fwd)(&s->fft_ctx);
    int n = 1 << NS(_logn);
    int offset = 0;
    int chunk_size = n/2 + 1;
    for (int block = 0; block < NS(_ols_nb_blocks); block++) {
        LOG("block %d\n", block);
        NS(_data_t) *ir_chunk_fft = s->filter[block].freq;
        NS(_real_t) ir_chunk_padded[n] = {};
        for (int i=0; i<chunk_size; i++) {
            int oi = offset + i;
            if (oi < nb_el) {
                LOG("  offset %d\n", oi);
                ir_chunk_padded[i] = impulse[oi];
            }
        }
        NS(_process_real)(&s->fft_ctx, ir_chunk_padded, ir_chunk_fft);

        offset += chunk_size;

    }

}
