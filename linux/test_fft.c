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
#define fft_uc_tag 0x1F320000 // LE 32bit floats
#include "ns_fft_io.h"
#undef NS

#define NS(name) ntt8##name
#define ntt8_logn 8
#define ntt8_field_mod (1+(1<<ntt8_logn))
#include "ns_galois.h"
ntt8_data_t ntt8_scale_fwd = 1;
ntt8_data_t ntt8_scale_rev = (1<<ntt8_logn);
#include "ns_fft.h"
#define ntt8_ols_nb_blocks 5
#include "ns_ols.h"
#define ntt8_uc_tag 0x15320000 // LE 32bit signed integers
#include "ns_fft_io.h"
#undef NS

#define NS(name) ntt4##name
#define ntt4_logn 4
#define ntt4_field_mod (1+(1<<ntt4_logn))
#include "ns_galois.h"
ntt4_data_t ntt4_scale_fwd = 1;
ntt4_data_t ntt4_scale_rev = (1<<ntt4_logn);
#include "ns_fft.h"
#define ntt4_ols_nb_blocks 5
#include "ns_ols.h"
#define ntt4_uc_tag 0x15320000
#include "ns_fft_io.h"
#undef NS




int main(int argc, char **argv) {
    if (argc >= 2) {
        const char *cmd = argv[1];
        if (!strcmp(cmd, "ntt8")) {
            ntt8_io_serve();
            return 0;
        }
        if (!strcmp(cmd, "ntt4")) {
            ntt4_io_serve();
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
