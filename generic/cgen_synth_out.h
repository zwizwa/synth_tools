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
    // loop index init
    I n0 = zero();
    // loop state init
    T l0 = zero();
    for(; n0 < 64; n0++) {
        // loop state snapshot
        T l1 = copy(l0);
        // loop body
        T l2 = copy(i->i0[n0]);
        // feedback state snapshot
        T l3 = copy(s->s0[n0]);
        // feedback body
        T l4 = add(l3, l2);
        T l5 = frac(l4);
        // feedback state update
        s->s0[n0] = l5;
        T l6 = add(l1, l3);
        // loop state update
        l0 = l6;
        // loop output
    }
    // function body
    // function outputs
    o->o0 = l0;
}
