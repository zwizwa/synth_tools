#include "cgen_lib.h"
struct synth_state {
    T s0[64];
    T s1[64];
};
struct synth_in {
    T i0[64];
};
struct synth_out {
    T o0[64];
};
static inline void synth_update(struct synth_state *s, const struct synth_in *i, struct synth_out *o) {
    // function body
    // loop state init
    I n0 = zero();
    T v5[64];
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
        T v0 = div(1, 64);
        T v1 = copy(i->i0[n0]);
        // feedback state snapshot
        T v2 = copy(s->s0[n0]);
        // feedback body
        // feedback state update
        // dst: #(struct:assign #(struct:var T (#(struct:dim #(struct:var I () n 0) 64)) s 0) () #(struct:var T () v 1))
        s->s0[n0] = v1;
        T v3 = sub(i->i0[n0], v2);
        T v4 = mul(v3, v0);
        // loop body output as var
        // loop state update
        // loop output
        // dst: #(struct:assign #(struct:var T (#(struct:dim #(struct:var I () n 0) 64)) v 5) (#(struct:var I () n 0)) #(struct:var T () v 4))
        v5[n0] = v4;
    }
    // loop state init
    I t0 = zero();
    // omit slice definition: T v18[64] is in o0
    T l0[64];
    // loop state init
    I n1 = zero();
    // omit slice definition: T v7[64] is in l0
    for(; n1 < 64; n1++) {
        // loop state snapshot
        // loop body
        // loop body output as var
        T v6 = copy(i->i0[n1]);
        // loop state update
        // loop output
        l0[n1] = v6; // expanded from: v7[n1] = v6
    }
    // loop-state-from!: omit slice assigment: l0 is v7
    for(; t0 < 64; t0++) {
        // loop state snapshot
        // loop state init
        I n2 = zero();
        T v9[64];
        for(; n2 < 64; n2++) {
            // loop state snapshot
            // loop body
            // loop body output as var
            T v8 = copy(l0[n2]);
            // loop state update
            // loop output
            // dst: #(struct:assign #(struct:var T (#(struct:dim #(struct:var I () n 2) 64)) v 9) (#(struct:var I () n 2)) #(struct:var T () v 8))
            v9[n2] = v8;
        }
        // loop body
        // loop state init
        I n3 = zero();
        // omit slice definition: T v17[64] is in l0
        T l1;
        // dst: #(struct:assign #(struct:var T () l 1) () 0)
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
            // dst: #(struct:assign #(struct:var T (#(struct:dim #(struct:var I () n 3) 64)) s 1) () #(struct:var T () v 14))
            s->s1[n3] = v14;
            T v15 = add(v10, v12);
            T v16 = add(v9[n3], v5[n3]);
            // loop body output as var
            // loop state update
            // dst: #(struct:assign #(struct:var T () l 1) () #(struct:var T () v 15))
            l1 = v15;
            // loop output
            l0[n3] = v16; // expanded from: v17[n3] = v16
        }
        // loop body output as var
        // loop state update
        // loop-state-update: omit slice assigment: l0 is v17
        // loop output
        o->o0[t0] = l1; // expanded from: v18[t0] = l1
    }
    // function outputs
    // top-out: omit slice assigment: o->o0 is v18
}
