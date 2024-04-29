#include "macros.h"
#include "cgen_synth_out.h"
#include "cgen_test_out.h"

int main(int argc, char **argv) {
    // State is initialized to zero.
    struct synth_state state = {};
    for (int i=0; i<20; i++) {
        // Input is an array of phase increments, one for each osc.
        // Constant here, but this is a signal, it can very with each
        // update.
        struct synth_in in = { .i0 = {.3, .0001} };
        // Output is a single audio sample.
        struct synth_out out;
        synth_update(&state, &in, &out);
        printf("%f\n", out.o0);
    }
}
