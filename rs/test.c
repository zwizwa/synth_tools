/* The idea is to be able to call the Rust code from C, so this is a
   test for that.  Some general notes:

   - it's easier to use arrays instead of linked lists.  Basically,
     avoid pointers in general and replace them with indexed data
     structures.
*/

#include "macros.h"
#include <stdint.h>

#include "mod_sequencer.c"

uint32_t add1(uint32_t);
void pattern_test(struct pattern_step *, size_t);

void test_binheap(uint16_t *, size_t);
void test_rotate(struct pattern_step *, size_t);

void synth_tools_rs_init(void);

int main(int argc, char **argv) {

    synth_tools_rs_init();


    struct sequencer s;
    sequencer_init(&s, NULL);

    {
        LOG("pattern:\n");
        struct pattern_step p[10] = {};
        pattern_test(p, ARRAY_SIZE(p));
        for (int i=0; i<ARRAY_SIZE(p); i++) {
            LOG("%d delay = %d\n", i, p[i].delay);
        }
    }
    {
        LOG("rotate:\n");
        struct pattern_step p[10] = {};
        int n = ARRAY_SIZE(p);
        test_rotate(p, n);
    }
}
