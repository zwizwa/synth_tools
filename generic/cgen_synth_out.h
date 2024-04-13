#include "cgen_lib.h"
struct synth_state {
    T s0[64];
};
struct synth_in {
    T i0[64];
};
struct synth_out {
    T o0;
};
static inline void synth_update(struct synth_state *s, const struct synth_in *i, struct synth_out *o) {
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    T l0 = zero();
    for(; n0 < 64; n0++) {
        // loop state snapshot
        T l1 = copy(l0);
        // loop body
        // feedback state snapshot
        T l2 = copy(s->s0[n0]);
        // feedback body
        T l3 = add(l2, i->i0[n0]);
        T l4 = frac(l3);
        // feedback state update
        s->s0[n0] = l4;
        T l5 = add(l1, l2);
        // loop state update
        l0 = l5;
        // loop output
    }
    // function outputs
    o->o0 = l0;
}
