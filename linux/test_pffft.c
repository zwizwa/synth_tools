/* Standalone test binary for pffft.
   Roll this into test_armv7.c later. */

#if 0
// use functions provided by libpffft.a
#include "pffft.h"
#define MOD_PFFFT_N 512
PFFFT_Setup *pffft_init(void) {
    return pffft_new_setup(MOD_PFFFT_N, PFFFT_REAL);
}
#else
// include the pffft code into this compilation unit
#include "mod_pffft.c"
static struct pffft_r512_static pffft_r512_static;
PFFFT_Setup *pffft_r512_init(void) {
    pffft_r512_static_init(&pffft_r512_static);
    return &pffft_r512_static.setup;
}
#endif

#include "macros.h"

int main(int argc, char **argv) {
    PFFFT_Setup *setup = pffft_r512_init();
    float input[512] = {1};
    float output[512] = {};
    float work[512] = {};
    pffft_direction_t direction = PFFFT_FORWARD;
    pffft_transform(setup, input, output, work, direction);
    LOG("test_pffft.c\n");
    for (int i=0; i<512; i++) {
        LOG("%f\n", output[i]);
    }
    return 0;
}
