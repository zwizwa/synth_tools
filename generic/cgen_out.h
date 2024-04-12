#ifndef CGEN_OUT_H
#define CGEN_OUT_H
#include "cgen_lib.h"
struct cgen_state {
};
struct cgen_in {
    T i0[64];
};
struct cgen_out {
    T o6;
};
static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {
    // function body
    // loop index init
    I n1 = zero();
    // loop state init
    T l5[3][4];
    for(; n1 < 3; n1++) {
        // (#(struct:dim #(struct:reg I () n 1) 3))
        // loop state snapshot
        // loop body
        // loop index init
        I n2 = zero();
        // loop state init
        T l4[4];
        for(; n2 < 4; n2++) {
            // (#(struct:dim #(struct:reg I () n 1) 3) #(struct:dim #(struct:reg I () n 2) 4))
            // loop state snapshot
            // loop body
            T l3 = mul(n1, n2);
            // loop state update
            // loop output
            l4[n2] = l3;
        }
        // loop state update
        // loop output
        // FIXME: removed array assignment, defining slice
    }
    // function outputs
    copy_array(o->o6, l5);
}
#endif
