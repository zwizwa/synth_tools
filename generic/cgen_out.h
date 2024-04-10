#ifndef CGEN_OUT_H
#define CGEN_OUT_H
#include "cgen_lib.h"
struct cgen_state {
    T s4[3];
};
struct cgen_in {
    T i0;
};
struct cgen_out {
    T o8;
};
static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {
    // function body
    // loop index init
    I n1 = zero();
    // loop state init
    T l2 = zero();
    for(; n1 < 3; n1++) {
        // (#(struct:dim #(struct:reg I () n 1) 3))
        // loop state snapshot
        T l3 = copy(l2);
        // loop body
        // feedback state snapshot
        T l5 = copy(s->s4[n1]);
        // feedback body
        T l6 = add(l5, i->i0);
        // feedback state update
        s->s4[n1] = l6;
        T l7 = add(l3, l5);
        // loop state update
        l2 = l7;
        // loop output
    }
    // function outputs
    o->o8 = l2;
}
#endif
