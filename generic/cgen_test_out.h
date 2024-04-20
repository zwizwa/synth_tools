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
    // feedback state snapshot
    T v0 = copy(s->s0);
    // feedback body
    // feedback state update
    s->s0 = i->i1;
    T v1 = sub(i->i1, v0);
    T v2 = div(v1, 64);
    // loop index init
    I t0 = zero();
    // loop state zero init
    T l0 = zero();
    // omit slice definition: T v10[64]
    // omit slice definition: T v11[64]
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
        // loop state update
        l0 = v9;
        // loop output
        // treat assignment as equivalence: v10[t0] == v6
        // treat assignment as equivalence: v11[t0] == v8
    }
    // function body
    // function outputs
    o->o0 = l0;
    // treat assignment as equivalence: o->o1 == v10
    // treat assignment as equivalence: o->o2 == v11
}
#include "cgen_lib.h"
struct matrix_state {
};
struct matrix_in {
};
struct matrix_out {
    T o0[3][4];
};
static inline void matrix_update(struct matrix_state *s, const struct matrix_in *i, struct matrix_out *o) {
    // loop index init
    I n0 = zero();
    // loop state zero init
    // omit slice definition: T v2[3][4]
    for(; n0 < 3; n0++) {
        // loop state snapshot
        // loop body
        // loop index init
        I n1 = zero();
        // loop state zero init
        // omit slice definition: T v1[4]
        for(; n1 < 4; n1++) {
            // loop state snapshot
            // loop body
            T v0 = mul(n0, n1);
            // loop state update
            // loop output
            // treat assignment as equivalence: v1[n1] == v0
        }
        // loop state update
        // loop output
        // treat assignment as equivalence: v2[n0] == v1
    }
    // function body
    // function outputs
    // treat assignment as equivalence: o->o0 == v2
}
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
    // loop index init
    I n0 = zero();
    // state initializer
    T l0 = copy(123);
    T l1 = copy(456);
    for(; n0 < 4; n0++) {
        // loop state snapshot
        T v0 = copy(l0);
        T v1 = copy(l1);
        // loop body
        // loop state update
        l0 = v0;
        l1 = v1;
        // loop output
    }
    // function body
    // function outputs
    o->o0 = l0;
    o->o1 = l1;
}
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
    // feedback state snapshot
    T v0 = copy(s->s0);
    // feedback body
    T v1 = add(v0, i->i0);
    // feedback state update
    s->s0 = v1;
    // function body
    // function outputs
    o->o0 = v0;
}
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
    // function body
    // function outputs
    o->o0 = v2;
}
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
    // loop index init
    I n0 = zero();
    // loop state zero init
    T l0 = zero();
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
        // loop state update
        l0 = v3;
        // loop output
    }
    // function body
    // function outputs
    o->o0 = l0;
}
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
    T v0 = div(1, 1024);
    // loop index init
    I n0 = zero();
    // loop state zero init
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
        // loop state update
        // loop output
        // treat assignment as equivalence: v5[n0] == v4
    }
    // loop index init
    I t0 = zero();
    // state initializer
    T l0 = copy(i->i0);
    // omit slice definition: T v13[1024]
    for(; t0 < 1024; t0++) {
        // loop state snapshot
        T v6 = copy(l0);
        // loop body
        // loop index init
        I n1 = zero();
        // loop state zero init
        T l1 = zero();
        T v12[64];
        for(; n1 < 64; n1++) {
            // loop state snapshot
            T v7 = copy(l1);
            // loop body
            // feedback state snapshot
            T v8 = copy(s->s1[n1]);
            // feedback body
            T v9 = add(v8, v6);
            // feedback state update
            s->s1[n1] = v9;
            T v10 = add(v7, v8);
            T v11 = add(v6, v5[n1]);
            // loop state update
            l1 = v10;
            // loop output
            // treat assignment as equivalence: v12[n1] == v11
        }
        // loop state update
        l0 = v12;
        // loop output
        // treat assignment as equivalence: v13[t0] == l1
    }
    // function body
    // function outputs
    // treat assignment as equivalence: o->o0 == v13
}
