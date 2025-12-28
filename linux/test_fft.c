#include "macros.h"
#include "assert_read.h"
#include "assert_write.h"
#define N 256

void process(const float *in, float *out, int n) {
    LOG("process begin %d\n", n);
    for(int i=0; i<n; i++) {
        out[i] = in[i] + 100;
    }
    LOG("process end %d\n", n);
}

int main(int argc, char **argv) {
    /* What protocol to use?  Start with raw floats. */
    float ibuf[N];
    float obuf[N];
    for(;;) {
        assert_read_fixed(0, ibuf, sizeof(ibuf));
        process(ibuf, obuf, N);
        assert_write(1, (const void*)obuf, sizeof(obuf));
    }
    return 0;
}
