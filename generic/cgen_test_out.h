/////////////////////////// integrator
#include "cgen_lib.h"
struct integrator_state {
    T s0;
};
struct integrator_in {
    T i0;
};
struct integrator_out {
    T o0;
};
static inline void integrator_update(struct integrator_state *s, const struct integrator_in *i, struct integrator_out *o) {
    // function body
    // feedback state snapshot
    T v0 = copy(s->s0);
    // feedback body
    T v1 = add(v0, i->i0);
    // feedback state update
    s->s0 = v1;
    // function outputs
    o->o0 = v0;
}

/////////////////////////// procproc
#include "cgen_lib.h"
struct procproc_state {
    T s0;
    T s1;
};
struct procproc_in {
    T i0;
};
struct procproc_out {
    T o0;
};
static inline void procproc_update(struct procproc_state *s, const struct procproc_in *i, struct procproc_out *o) {
    // function body
    // feedback state snapshot
    T v0 = copy(s->s0);
    // feedback body
    T v1 = add(v0, i->i0);
    // feedback state update
    s->s0 = v1;
    // feedback state snapshot
    T v2 = copy(s->s1);
    // feedback body
    T v3 = add(v2, v0);
    // feedback state update
    s->s1 = v3;
    // function outputs
    o->o0 = v2;
}

/////////////////////////// sumramp
#include "cgen_lib.h"
struct sumramp_state {
    T s0[3];
};
struct sumramp_in {
    T i0;
};
struct sumramp_out {
    T o0;
};
static inline void sumramp_update(struct sumramp_state *s, const struct sumramp_in *i, struct sumramp_out *o) {
    // function body
    I n0 = zero();
    T l0;
    l0 = 0;
    for(; n0 < 3; n0++) {
        // loop state snapshot
        T v0 = copy(l0);
        // loop body
        // feedback state snapshot
        T v1 = copy(s->s0[n0]);
        // feedback body
        T v2 = add(v1, i->i0);
        // feedback state update
        s->s0[n0] = v2;
        T v3 = add(v0, v1);
        // loop body output as var
        // loop state update
        l0 = v3;
        // loop output
    }
    // function outputs
    o->o0 = l0;
}

/////////////////////////// matrix
#include "cgen_lib.h"
struct matrix_state {
};
struct matrix_in {
};
struct matrix_out {
    T o0[3][4];
};
static inline void matrix_update(struct matrix_state *s, const struct matrix_in *i, struct matrix_out *o) {
    // function body
    I n0 = zero();
    // omit slice definition: T v2[3][4]
    for(; n0 < 3; n0++) {
        // loop state snapshot
        // loop body
        I n1 = zero();
        // omit slice definition: T v1[4]
        for(; n1 < 4; n1++) {
            // loop state snapshot
            // loop body
            T v0 = mul(n0, n1);
            // loop body output as var
            // loop state update
            // loop output
            o->o0[n0][n1] = v0; // expanded from: v1[n1] = v0
        }
        // loop body output as var
        // loop state update
        // loop output
        // loop-out: treat assignment as equivalence: v2[n0] == v1
    }
    // function outputs
    // top-out: treat assignment as equivalence: o->o0 == v2
}

/////////////////////////// timeloop
#include "cgen_lib.h"
struct timeloop_state {
    T s0;
    T s1;
    T s2;
};
struct timeloop_in {
    T i0[64];
    T i1;
};
struct timeloop_out {
    T o0;
    T o1[64];
    T o2[64];
};
static inline void timeloop_update(struct timeloop_state *s, const struct timeloop_in *i, struct timeloop_out *o) {
    // function body
    // feedback state snapshot
    T v0 = copy(s->s0);
    // feedback body
    // feedback state update
    s->s0 = i->i1;
    T v1 = sub(i->i1, v0);
    T v2 = div(v1, 64);
    I t0 = zero();
    // omit slice definition: T v10[64]
    // omit slice definition: T v11[64]
    T l0;
    l0 = 0;
    for(; t0 < 64; t0++) {
        // loop state snapshot
        T v3 = copy(l0);
        // loop body
        T v4 = copy(i->i0[t0]);
        // feedback state snapshot
        T v5 = copy(s->s1);
        // feedback body
        T v6 = add(v5, v4);
        // feedback state update
        s->s1 = v6;
        // feedback state snapshot
        T v7 = copy(s->s2);
        // feedback body
        T v8 = add(v7, v6);
        // feedback state update
        s->s2 = v8;
        T v9 = add(v3, v2);
        // loop body output as var
        // loop state update
        l0 = v9;
        // loop output
        o->o1[t0] = v6; // expanded from: v10[t0] = v6
        o->o2[t0] = v8; // expanded from: v11[t0] = v8
    }
    // function outputs
    o->o0 = l0;
    // top-out: treat assignment as equivalence: o->o1 == v10
    // top-out: treat assignment as equivalence: o->o2 == v11
}

/////////////////////////// loopinit
#include "cgen_lib.h"
struct loopinit_state {
};
struct loopinit_in {
};
struct loopinit_out {
    T o0;
    T o1;
};
static inline void loopinit_update(struct loopinit_state *s, const struct loopinit_in *i, struct loopinit_out *o) {
    // function body
    I n0 = zero();
    T l0;
    T l1;
    l0 = 123;
    l1 = 456;
    for(; n0 < 4; n0++) {
        // loop state snapshot
        T v0 = copy(l0);
        T v1 = copy(l1);
        // loop body
        // loop body output as var
        // loop state update
        l0 = v0;
        l1 = v1;
        // loop output
    }
    // function outputs
    o->o0 = l0;
    o->o1 = l1;
}

/////////////////////////// loopstateinit
#include "cgen_lib.h"
struct loopstateinit_state {
};
struct loopstateinit_in {
    T i0[64];
};
struct loopstateinit_out {
    T o0[10];
};
static inline void loopstateinit_update(struct loopstateinit_state *s, const struct loopstateinit_in *i, struct loopstateinit_out *o) {
    // function body
    I n1 = zero();
    // omit slice definition: T v5[10]
    T l0[64];
    I n0 = zero();
    // omit slice definition: T v1[64]
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
        // loop body output as var
        T v0 = copy(i->i0[n0]);
        // loop state update
        // loop output
        l0[n0] = v0; // expanded from: v1[n0] = v0
    }
    // ls-from!: treat assignment as equivalence: l0 == v1
    for(; n1 < 10; n1++) {
        // loop state snapshot
        I n2 = zero();
        // omit slice definition: T v3[64]
        for(; n2 < 64; n2++) {
            // loop state snapshot
            // loop body
            // loop body output as var
            T v2 = copy(l0[n2]);
            // loop state update
            // loop output
            l0[n2] = v2; // expanded from: v3[n2] = v2
        }
        // loop body
        // loop body output as var
        T v4 = copy(123);
        // loop state update
        // loop-state-update: treat assignment as equivalence: l0 == v3
        // loop output
        o->o0[n1] = v4; // expanded from: v5[n1] = v4
    }
    // function outputs
    // top-out: treat assignment as equivalence: o->o0 == v5
}

/////////////////////////// interpol
#include "cgen_lib.h"
struct interpol_state {
    T s0[64];
    T s1[64];
};
struct interpol_in {
    T i0[64];
};
struct interpol_out {
    T o0[1024];
};
static inline void interpol_update(struct interpol_state *s, const struct interpol_in *i, struct interpol_out *o) {
    // function body
    T v0 = div(1, 1024);
    I n0 = zero();
    T v5[64];
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
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
    // omit slice definition: T v17[1024]
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
        // omit slice definition: T v16[64]
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
            // feedback state update
            s->s1[n3] = v13;
            T v14 = add(v10, v12);
            T v15 = add(v9[n3], v5[n3]);
            // loop body output as var
            // loop state update
            l1 = v14;
            // loop output
            l0[n3] = v15; // expanded from: v16[n3] = v15
        }
        // loop body output as var
        // loop state update
        // loop-state-update: treat assignment as equivalence: l0 == v16
        // loop output
        o->o0[t0] = l1; // expanded from: v17[t0] = l1
    }
    // function outputs
    // top-out: treat assignment as equivalence: o->o0 == v17
}

