#include "cgen_lib.h"
struct timeloop_state {
    T s0;
    T s1;
};
struct timeloop_in {
    T i0[64];
};
struct timeloop_out {
    T o0[64];
    T o1[64];
};
static inline void timeloop_update(struct timeloop_state *s, const struct timeloop_in *i, struct timeloop_out *o) {
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    // omit slice definition: T l4[64]
    // omit slice definition: T l5[64]
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
        // feedback state snapshot
        T l0 = copy(s->s0);
        // feedback body
        T l1 = add(l0, i->i0[n0]);
        // feedback state update
        s->s0 = l1;
        // feedback state snapshot
        T l2 = copy(s->s1);
        // feedback body
        T l3 = add(l2, l1);
        // feedback state update
        s->s1 = l3;
        // loop state update
        // loop output
        o->o0[n0] = l1; // expanded from: l4[n0] = l1
        o->o1[n0] = l3; // expanded from: l5[n0] = l3
    }
    // function outputs
    // treat assignment as equivalence: o->o0 == l4
    // treat assignment as equivalence: o->o1 == l5
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
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    // omit slice definition: T l2[3][4]
    for(; n0 < 3; n0++) {
        // loop state snapshot
        // loop body
        // loop index init
        I n1 = zero();
        // loop state init
        // omit slice definition: T l1[4]
        for(; n1 < 4; n1++) {
            // loop state snapshot
            // loop body
            T l0 = mul(n0, n1);
            // loop state update
            // loop output
            o->o0[n0][n1] = l0; // expanded from: l1[n1] = l0
        }
        // loop state update
        // loop output
        // treat assignment as equivalence: l2[n0] == l1
    }
    // function outputs
    // treat assignment as equivalence: o->o0 == l2
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
    // function body
    // feedback state snapshot
    T l0 = copy(s->s0);
    // feedback body
    T l1 = add(l0, i->i0);
    // feedback state update
    s->s0 = l1;
    // function outputs
    o->o0 = l0;
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
    // function body
    // feedback state snapshot
    T l0 = copy(s->s0);
    // feedback body
    T l1 = add(l0, i->i0);
    // feedback state update
    s->s0 = l1;
    // feedback state snapshot
    T l2 = copy(s->s1);
    // feedback body
    T l3 = add(l2, l0);
    // feedback state update
    s->s1 = l3;
    // function outputs
    o->o0 = l2;
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
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    T l0 = zero();
    for(; n0 < 3; n0++) {
        // loop state snapshot
        T l1 = copy(l0);
        // loop body
        // feedback state snapshot
        T l2 = copy(s->s0[n0]);
        // feedback body
        T l3 = add(l2, i->i0);
        // feedback state update
        s->s0[n0] = l3;
        T l4 = add(l1, l2);
        // loop state update
        l0 = l4;
        // loop output
    }
    // function outputs
    o->o0 = l0;
}
