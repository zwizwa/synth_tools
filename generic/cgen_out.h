#ifndef CGEN_OUT_H
#define CGEN_OUT_H
#include "cgen_lib.h"
struct cgen_state {
    T s3[64];
};
struct cgen_in {
    T i0[64];
};
struct cgen_out {
    T o8;
};
static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    T l1 = zero();
    for(; n0 < 64; n0++) {
        // (#(struct:dim #(struct:reg I () n 0) 64))
        // loop state snapshot
        T l2 = copy(l1);
        // loop body
        // feedback state snapshot
        T l4 = copy(s->s3[n0]);
        // feedback body
        T l5 = add(l4, i->i0[n0]);
        T l6 = frac(l5);
        // feedback state update
        s->s3[n0] = l6;
        T l7 = add(l2, l4);
        // loop state update
        l1 = l7;
        // loop output
    }
    // function outputs
    o->o8 = l1;
}
#endif
