#ifndef CGEN_OUT_H
#define CGEN_OUT_H
#include "cgen_lib.h"
struct cgen_state {
    T s1;
};
struct cgen_in {
    T i0;
};
struct cgen_out {
    T o5;
};
static inline void cgen_update(struct cgen_state *s, const struct cgen_in *i, struct cgen_out *o) {
    // function body
    // feedback state snapshot
    T l2 = copy(s->s1);
    // feedback body
    T l3 = add(l2, i->i0);
    T l4 = frac(l3);
    // feedback state update
    s->s1 = l4;
    // function outputs
    o->o5 = l2;
}
#endif
