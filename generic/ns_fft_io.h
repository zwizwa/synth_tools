// Self-contained "RPC server" handling uc_tools tag=0x1EEE protocol
// on stdin/stdout to drive FFT/IFFT. Used in Haskell unit tests.  See
// test_fft.c for top level wrapper.

#include "macros.h"
#include "uct_byteswap.h"
#include "assert_read.h"
#include "assert_write.h"

void NS(_io_serve)(void) {
    struct NS(_ctx) ctx = {};
    NS(_init_ctx)(&ctx);
    int n = 1 << ctx.top_logn;
    NS(_data_t) ibuf[n];
    NS(_data_t) obuf[n];
    for(;;) {
        /* Header is 3 words: 2 words BE uc_tools header and one LE
           command.  Read it all at once. */
        int32_t in_hdr[3] = { };
        assert_read_fixed(0, in_hdr, sizeof(in_hdr));
        /* The uc_tools header size and tag are in BE.  Check the tag
           and make sure the size corresponds to what we are expecting
           here. */
        ASSERT(0x1EEE0000 == SWAP_U32(in_hdr[1]));
        uint32_t nb_float = (SWAP_U32(in_hdr[0]) / 4) - 2;
        // LOG("nb_float = %d\n", nb_float);
        ASSERT(nb_float == n);

        /* The rest is LE */
        int32_t cmd = in_hdr[2];
        assert_read_fixed(0, ibuf, sizeof(ibuf));

        /* Perform requested operation. */
        //LOG("test_ntt op 0x%x\n", cmd);
        switch(cmd) {
        case 0x100: NS(_dir_fwd)(&ctx); break;
        case 0x101: NS(_dir_rev)(&ctx); break;
        default : ERROR("bad ntt opcode 0x%x\n", cmd); break;
        }
        NS(_process)(&ctx, ibuf, obuf);

        /* Send output */
        struct {
            // uc_tools compatible framing + tag
            uint32_t size;
            uint16_t tag;
            // subtag to round it to 32 bit multiple
            uint16_t subtag;
        } out_hdr = {
            .size = SWAP_U32(4 * (2 + n)),
            .tag  = SWAP_U16(0xA100),
        };
        assert_write(1, (const void*)&out_hdr, sizeof(out_hdr));
        assert_write(1, (const void*)obuf, sizeof(obuf));
    }
}
