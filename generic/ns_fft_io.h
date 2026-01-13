// Self-contained "RPC server" handling uc_tools tag=0x1EEE protocol
// on stdin/stdout to drive FFT/IFFT. Used in Haskell unit tests.  See
// test_fft.c for top level wrapper.

// Note that the point here is to run QuickCheck tests.  In
// production, the FFT sizes are going to be fixed so we do the same
// for test to keep things simple.  The ns_* modules area all
// specialized to a single vector size.

#include "macros.h"
#include "uct_byteswap.h"
#include "assert_read.h"
#include "assert_write.h"

//struct NS(_fft_io_test) {
//};

void NS(_io_serve_reply)(void *buf, uint32_t nb_bytes) {
    /* Send output */
    struct {
        // uc_tools compatible framing + tag
        uint32_t size;
        uint16_t tag;
        // subtag to round it to 32 bit multiple
        uint16_t subtag;
    } out_hdr = {
        .size = SWAP_U32(4 + nb_bytes),
        .tag  = SWAP_U16(0xA100),
    };
    assert_write(1, (const void*)&out_hdr, sizeof(out_hdr));
    if (nb_bytes > 0) {
        assert_write(1, buf, nb_bytes);
    }
}

void NS(_io_serve)(void) {

    struct NS(_ctx) ctx = {};
    NS(_init_ctx)(&ctx);

    struct NS(_ols_state) ols = {};
    ols.fft_ctx = ctx;

    //NS(_fft_io_test) test = {};

    /* FFT sizes are hardcoded.  For each message we need to check
       that the size is correct. */
    int32_t logn = ctx.top_logn;
    int32_t n = 1 << logn;

    for(;;) {
        /* Header is 3 words: 2 words BE uc_tools header and one LE
           command.  Read it all at once. */
        int32_t in_hdr[3] = { };
        assert_read_fixed(0, in_hdr, sizeof(in_hdr));
        /* The uc_tools header size and tag are in BE.  Check the tag
           and make sure the size corresponds to what we are expecting
           here. */
        ASSERT(0x1EEE0000 == SWAP_U32(in_hdr[1]));
        uint32_t nb_el = (SWAP_U32(in_hdr[0]) / 4) - 2;
        // LOG("nb_el = %d\n", nb_el);

        /* The rest is LE */
        int32_t cmd = in_hdr[2];

        /* Perform requested operation. */
        // LOG("test_ntt op 0x%x\n", cmd);
        switch(cmd) {
        case 0x100: /* fft */
        case 0x101: /* ifft */ {
            ASSERT(nb_el == n);
            NS(_data_t) ibuf[nb_el];
            NS(_data_t) obuf[nb_el];
            assert_read_fixed(0, ibuf, sizeof(ibuf));
            if (cmd == 0x100) {
                NS(_dir_fwd)(&ctx);
            }
            else {
                NS(_dir_rev)(&ctx);
            }
            NS(_process)(&ctx, ibuf, obuf);

            NS(_io_serve_reply)(obuf, sizeof(obuf));
            break;
        }
        case 0x102: /* ols tick */ {
            //LOG("ols tick %d begin\n", nb_el);
            ASSERT(nb_el == n/2);
            NS(_real_t) ibuf[nb_el];
            NS(_real_t) obuf[nb_el] = {};
            assert_read_fixed(0, ibuf, sizeof(ibuf));
            NS(_ols)(&ols, ibuf, obuf);
            NS(_io_serve_reply)(obuf, sizeof(obuf));
            //LOG("ols tick %d begin end\n", nb_el);
            break;
        }
        case 0x103: /* ols init */ {
            //LOG("ols init %d begin\n", nb_el);
            int nb_impulse = NS(_ols_nb_blocks) * (n/2 + 1);
            ASSERT(nb_el <= nb_impulse);
            NS(_real_t) ibuf[nb_impulse] = {};
            assert_read_fixed(0, ibuf, nb_el * sizeof(ibuf[0]));
            NS(_ols_init)(&ols, ibuf, nb_el);
            //LOG("ols init %d end\n", nb_el);
            NS(_io_serve_reply)(NULL, 0);
            break;
        }
        case 0x104: /* get config */ {
            LOG("ols config, logn=%d, size=%d\n", logn, n);
            ASSERT(nb_el == 0); // nothing left to read
            NS(_io_serve_reply)(&logn, sizeof(logn));
            break;
        }
        default:
            ERROR("bad ntt opcode 0x%x\n", cmd); break;
        }

    }
}
