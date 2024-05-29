#include "macros.h"
#include "cgen_synth_out.h"

int main(int argc, char **argv) {
    // State is initialized to zero.
    struct synth_state state = {};
    // Input is an array of phase increments, one for each osc,
    // provided once per block.
    struct synth_in in = { .i0 = {.3, .0001} };
    // Output is a block of audio samples.
    struct synth_out out;
    synth_update(&state, &in, &out);
    for (int i=0; i < ARRAY_SIZE(out.o0); i++) {
        printf("(%4d) %f\n", i, out.o0[i]);
    }
}
