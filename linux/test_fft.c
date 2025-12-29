#include "macros.h"

#define NS(name) fft##name
#define fft_logn 8
#include "ns_complex.h"
#include "ns_fft.h"
#undef NS

#define NS(name) ntt##name
#define ntt_logn 8
#define ntt_field_mod 257
#include "ns_galois.h"
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
        assert_read_fixed(0, ibuf, sizeof(ibuf));
        ntt_process(&ntt_ctx, ibuf, obuf, LOGN);
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
        assert_read_fixed(0, ibuf, sizeof(ibuf));
        fft_process(&fft_ctx, ibuf, obuf, LOGN);
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
