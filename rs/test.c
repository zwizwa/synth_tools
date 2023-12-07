/* The idea is to be able to call the Rust code from C, so this is a
   test for that. */
#include "macros.h"
#include <stdint.h>

#include "mod_sequencer.c"

uint32_t add1(uint32_t);
void pattern_rotate(struct pattern_step *, size_t);

int main(int argc, char **argv) {
    /* For Rust it's easier to use arrays instead of linked lists.  */
    struct pattern_step p[10] = {};
    pattern_rotate(p, ARRAY_SIZE(p));
    for (int i=0; i<ARRAY_SIZE(p); i++) {
        LOG("%d delay = %d\n", i, p[i].delay);
    }
}
