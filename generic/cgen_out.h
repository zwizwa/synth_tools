#ifndef CGEN_OUT_H
#define CGEN_OUT_H
#include "cgen_lib.h"
struct cgen_state {
};
struct cgen_in {
    T i0[64];
};
struct cgen_out {
    T o0;
};
static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {
    // function body
    // loop index init
    I n0 = zero();
    // loop state init
    T l2[3][4];
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
            l2[n0][n1] = l0; // expanded from: l1[n1] = l0
        }
        // loop state update
        // loop output
        // treat assignment as equivalence: l2[n0] = l1
    }
    // function outputs
    copy_array(o->o0, l2);
}
#endif
