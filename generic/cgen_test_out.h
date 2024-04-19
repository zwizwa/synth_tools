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
    T l0 = copy(s->s0);
    // feedback body
    // feedback state update
    s->s0 = i->i1;
    T l1 = sub(i->i1, l0);
    T l2 = div(l1, 64);
    // loop index init
    I t0 = zero();
    // loop state zero init
    T l3 = zero();
    // omit slice definition: T l11[64]
    // omit slice definition: T l12[64]
    for(; t0 < 64; t0++) {
        // loop state snapshot
        T l4 = copy(l3);
        // loop body
        T l5 = copy(i->i0[t0]);
        // feedback state snapshot
        T l6 = copy(s->s1);
        // feedback body
        T l7 = add(l6, l5);
        // feedback state update
        s->s1 = l7;
        // feedback state snapshot
        T l8 = copy(s->s2);
        // feedback body
        T l9 = add(l8, l7);
        // feedback state update
        s->s2 = l9;
        T l10 = add(l4, l2);
        // loop state update
        l3 = l10;
        // loop output
        // treat assignment as equivalence: l11[t0] == l7
        // treat assignment as equivalence: l12[t0] == l9
    }
    // function body
    // function outputs
    o->o0 = l3;
    // treat assignment as equivalence: o->o1 == l11
    // treat assignment as equivalence: o->o2 == l12
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
    // omit slice definition: T l2[3][4]
    for(; n0 < 3; n0++) {
        // loop state snapshot
        // loop body
        // loop index init
        I n1 = zero();
        // loop state zero init
        // omit slice definition: T l1[4]
        for(; n1 < 4; n1++) {
            // loop state snapshot
            // loop body
            T l0 = mul(n0, n1);
            // loop state update
            // loop output
            // treat assignment as equivalence: l1[n1] == l0
        }
        // loop state update
        // loop output
        // treat assignment as equivalence: l2[n0] == l1
    }
    // function body
    // function outputs
    // treat assignment as equivalence: o->o0 == l2
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
        T l2 = copy(l0);
        T l3 = copy(l1);
        // loop body
        // loop state update
        l0 = l2;
        l1 = l3;
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
    T l0 = copy(s->s0);
    // feedback body
    T l1 = add(l0, i->i0);
    // feedback state update
    s->s0 = l1;
    // function body
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
    // function body
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
    // loop index init
    I n0 = zero();
    // loop state zero init
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
    // function body
    // function outputs
    o->o0 = l0;
}
#include "cgen_lib.h"
struct interpol_state {
    T s0[64];
};
struct interpol_in {
    T i0[64];
};
struct interpol_out {
    T o0[1024];
};
static inline void interpol_update(struct interpol_state *s, const struct interpol_in *i, struct interpol_out *o) {
    T l0 = div(1, 1024);
    // loop index init
    I n0 = zero();
    // loop state zero init
    T l5[64];
    for(; n0 < 64; n0++) {
        // loop state snapshot
        // loop body
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
        // treat assignment as equivalence: l5[n0] == l4
    }
    // loop index init
    I t0 = zero();
    // state initializer
    T l6 = copy(i->i0);
    // omit slice definition: T l9[1024]
    for(; t0 < 1024; t0++) {
        // loop state snapshot
        T l7 = copy(l6);
        // loop body
        T l8 = copy(1024);
        // loop state update
        l6 = l7;
        // loop output
        o->o0[t0] = 1024; // expanded from: l9[t0] = 1024
    }
    // function body
    // function outputs
    // treat assignment as equivalence: o->o0 == l9
}
