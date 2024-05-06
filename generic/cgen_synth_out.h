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
    I n0 = zero();
    T v5[64];
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
        T v0 = div(1, 1024);
        T v1 = copy(i->i0[n0]);
        // feedback state snapshot
        T v2 = copy(s->s0[n0]);
        // feedback body
        // feedback state update
        s->s0[n0] = v1;
        T v3 = sub(i->i0[n0], v2);
        T v4 = mul(v3, v0);
        // loop body output as var
        // loop state update
        // loop output
        v5[n0] = v4;
    }
    I t0 = zero();
    // omit slice definition: T v18[1024]
    T l0[64];
    I n1 = zero();
    // omit slice definition: T v7[64]
    for(; n1 < 64; n1++) {
        // loop state snapshot
        // loop body
        // loop body output as var
        T v6 = copy(i->i0[n1]);
        // loop state update
        // loop output
        l0[n1] = v6; // expanded from: v7[n1] = v6
    }
    // ls-from!: treat assignment as equivalence: l0 == v7
    for(; t0 < 1024; t0++) {
        // loop state snapshot
        I n2 = zero();
        T v9[64];
        for(; n2 < 64; n2++) {
            // loop state snapshot
            // loop body
            // loop body output as var
            T v8 = copy(l0[n2]);
            // loop state update
            // loop output
            v9[n2] = v8;
        }
        // loop body
        I n3 = zero();
        // omit slice definition: T v17[64]
        T l1;
        l1 = 0;
        for(; n3 < 64; n3++) {
            // loop state snapshot
            T v10 = copy(l1);
            // loop body
            T v11 = copy(v9[n3]);
            // feedback state snapshot
            T v12 = copy(s->s1[n3]);
            // feedback body
            T v13 = add(v12, v11);
            T v14 = frac(v13);
            // feedback state update
            s->s1[n3] = v14;
            T v15 = add(v10, v12);
            T v16 = add(v9[n3], v5[n3]);
            // loop body output as var
            // loop state update
            l1 = v15;
            // loop output
            l0[n3] = v16; // expanded from: v17[n3] = v16
        }
        // loop body output as var
        // loop state update
        // loop-state-update: treat assignment as equivalence: l0 == v17
        // loop output
        o->o0[t0] = l1; // expanded from: v18[t0] = l1
    }
    // function body
    // function outputs
    // top-out: treat assignment as equivalence: o->o0 == v18
}
