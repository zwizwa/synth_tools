#include "mod_fft.c"

#include "macros.h"
#include "assert_read.h"
#include "assert_write.h"

#define LOGN 8
#define N (1<<LOGN)

struct fft_data fft_data = {};

int main(int argc, char **argv) {

    /* Run the initial bringup test. */
    test_fft();

    init_fft_data(&fft_data);

    /* What protocol to use?  Start with raw floats. */
    struct complex ibuf[N];
    struct complex obuf[N];
    for(;;) {
        assert_read_fixed(0, ibuf, sizeof(ibuf));
        fft_p2(&fft_data, ibuf, obuf, LOGN);
        assert_write(1, (const void*)obuf, sizeof(obuf));
    }
    return 0;
}
