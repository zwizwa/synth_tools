#include "cgen_lib.h"
struct synth_state {
    T s0[64];
};
struct synth_in {
    T i0[64];
};
struct synth_out {
    T o0[1024];
};
static inline void synth_update(struct synth_state *s, const struct synth_in *i, struct synth_out *o) {
    I t0 = zero();
    // omit slice definition: T v6[1024]
    for(; t0 < 1024; t0++) {
        // loop state snapshot
        // loop body
        I n0 = zero();
        T l0;
        l0 = 0;
        for(; n0 < 64; n0++) {
            // loop state snapshot
            T v0 = copy(l0);
            // loop body
            T v1 = copy(i->i0[n0]);
            // feedback state snapshot
            T v2 = copy(s->s0[n0]);
            // feedback body
            T v3 = add(v2, v1);
            T v4 = frac(v3);
            // feedback state update
            s->s0[n0] = v4;
            T v5 = add(v0, v2);
            // loop body output as var
            // loop state update
            l0 = v5;
            // loop output
        }
        // loop body output as var
        // loop state update
        // loop output
        o->o0[t0] = l0; // expanded from: v6[t0] = l0
    }
    // function body
    // function outputs
    // top-out: treat assignment as equivalence: o->o0 == v6
}
