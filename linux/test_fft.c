#include "macros.h"



#define NS(name) fft##name
#define fft_logn 8
#define fft_scale_fwd fft_scale_fwd
#include "ns_complex.h"
fft_data_t fft_scale_fwd = { .re = 1, .im = 0 };
fft_data_t fft_scale_rev = { .re = 1, .im = 0 }; // FIXME
#include "ns_fft.h"
#define fft_ols_nb_blocks 5
#include "ns_ols.h"
#include "ns_fft_io.h"
#undef NS

#define NS(name) ntt##name
#define ntt_logn 8
#define ntt_field_mod 257
#include "ns_galois.h"
ntt_data_t ntt_scale_fwd = 1;
ntt_data_t ntt_scale_rev = 256;
#include "ns_fft.h"
#define ntt_ols_nb_blocks 5
#include "ns_ols.h"
#include "ns_fft_io.h"
#undef NS



int main(int argc, char **argv) {
    if (argc >= 2) {
        const char *cmd = argv[1];
        if (!strcmp(cmd, "ntt")) {
            ntt_io_serve();
            return 0;
        }
        if (!strcmp(cmd, "fft")) {
            fft_io_serve();
            return 0;
        }
        LOG("bad command: %s\n", cmd);
    }
    else {
        LOG("command not specified\n");
        return 1;
    }
}
