#include "cgen_lib.h"
struct synth_state {
    T s0[64];
    T s1[64];
};
struct synth_in {
    T i0[64];
};
struct synth_out {
    T o0[1024];
};
static inline void synth_update(struct synth_state *s, const struct synth_in *i, struct synth_out *o) {
    // loop index init
    I n0 = zero();
    // loop state init
    T l5[64];
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
        T l0 = div(1, 1024);
        T l1 = copy(i->i0[n0]);
        // feedback state snapshot
        T l2 = copy(s->s0[n0]);
        // feedback body
        // feedback state update
        s->s0[n0] = l1;
        T l3 = sub(i->i0[n0], l2);
        T l4 = mul(l3, l0);
        // loop state update
        // loop output
        l5[n0] = l4
    }
    // loop index init
    I t0 = zero();
    // loop state init
    // omit slice definition: T l13[1024]
    for(; t0 < 1024; t0++) {
        // loop state snapshot
        // loop body
        // loop index init
        I n1 = zero();
        // loop state init
        T l6 = zero();
        for(; n1 < 64; n1++) {
            // loop state snapshot
            T l7 = copy(l6);
            // loop body
            T l8 = copy(i->i0[n1]);
            // feedback state snapshot
            T l9 = copy(s->s1[n1]);
            // feedback body
            T l10 = add(l9, l8);
            T l11 = frac(l10);
            // feedback state update
            s->s1[n1] = l11;
            T l12 = add(l7, l9);
            // loop state update
            l6 = l12;
            // loop output
        }
        // loop state update
        // loop output
        o->o0[t0] = l6; // expanded from: l13[t0] = l6
    }
    // function body
    // function outputs
    // treat assignment as equivalence: o->o0 == l13
}
