#ifndef CGEN_OUT_H
#define CGEN_OUT_H
#include "cgen_lib.h"
struct cgen_state {
};
struct cgen_in {
    T i0[64];
};
struct cgen_out {
    T o5;
};
static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    T l4[3][4];
    for(; n0 < 3; n0++) {
        // (#(struct:dim #(struct:reg I () n 0) 3))
        // loop state snapshot
        // loop body
        // loop index init
        I n1 = zero();
        // loop state init
        T l3[4];
        for(; n1 < 4; n1++) {
            // (#(struct:dim #(struct:reg I () n 0) 3) #(struct:dim #(struct:reg I () n 1) 4))
            // loop state snapshot
            // loop body
            T l2 = mul(n0, n1);
            // loop state update
            // loop output
            l3[n1] = l2;
        }
        // loop state update
        // loop output
        // FIXME: array assignment
        l4[n0] = l3;
    }
    // function outputs
    copy_array(o->o5, l4);
}
#endif
