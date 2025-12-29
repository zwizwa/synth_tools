#include "macros.h"

#define NS(name) fft##name
#define fft_logn 8
#define fft_scale_fwd fft_scale_fwd
#include "ns_complex.h"
fft_data_t fft_scale_fwd = { .re = 1, .im = 0 };
fft_data_t fft_scale_rev = { .re = 1, .im = 0 }; // FIXME
#include "ns_fft.h"
#undef NS

#define NS(name) ntt##name
#define ntt_logn 8
#define ntt_field_mod 257
#include "ns_galois.h"
ntt_data_t ntt_scale_fwd = 1;
ntt_data_t ntt_scale_rev = 256;
#include "ns_fft.h"
#undef NS


#include "assert_read.h"
#include "assert_write.h"

#define LOGN 8
#define N (1<<LOGN)

/* Lookup tables constructed at startup. */
struct fft_ctx fft_ctx = {};
struct ntt_ctx ntt_ctx = {};


int test_ntt(void) {
    LOG("test_ntt()\n");

    ntt_init_ctx(&ntt_ctx);

    ntt_data_t ibuf[N];
    ntt_data_t obuf[N];
    for(;;) {
        int32_t hdr[1] = {};
        assert_read_fixed(0, hdr, sizeof(hdr));
        assert_read_fixed(0, ibuf, sizeof(ibuf));
        //LOG("test_ntt op 0x%x\n", hdr[0]);
        switch(hdr[0]) {
        case 0x100:
            /* Direction to traverse coefficients + scaling. */
            ntt_ctx.direction = -1;
            ntt_ctx.scale = ntt_scale_fwd;
            break;
        case 0x101:
            ntt_ctx.direction = 1;
            ntt_ctx.scale = ntt_scale_rev;
            break;
        default : ERROR("bad ntt opcode 0x%x\n", hdr[0]); break;
        }
        ntt_process(&ntt_ctx, ibuf, obuf);
        assert_write(1, (const void*)obuf, sizeof(obuf));
    }
    return 0;

    return 0;
}

int test_fft(void) {
    LOG("test_fft()\n");

    fft_init_ctx(&fft_ctx);

    /* What protocol to use?  Start with raw floats. */
    fft_data_t ibuf[N];
    fft_data_t obuf[N];
    for(;;) {
        int32_t hdr[1] = {};
        assert_read_fixed(0, hdr, sizeof(hdr));
        assert_read_fixed(0, ibuf, sizeof(ibuf));
        fft_process(&fft_ctx, ibuf, obuf); break;
        assert_write(1, (const void*)obuf, sizeof(obuf));
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        const char *cmd = argv[1];
        if (!strcmp(cmd, "ntt")) { return test_ntt(); }
        if (!strcmp(cmd, "fft")) { return test_fft(); }
        LOG("bad command: %s\n", cmd);
    }
    else {
        LOG("command not specified\n");
        return 1;
    }
}
