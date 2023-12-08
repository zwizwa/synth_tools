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

int main(int argc, char **argv) {
    {
        struct pattern_step p[10] = {};
        pattern_test(p, ARRAY_SIZE(p));
        for (int i=0; i<ARRAY_SIZE(p); i++) {
            LOG("pattern:\n");
            LOG("%d delay = %d\n", i, p[i].delay);
        }
    }
    {
        uint16_t rv[3];
        int n = ARRAY_SIZE(rv);
        test_binheap(rv, n);
        for (int i=0; i<n; i++) {
            LOG("pattern:\n");
            LOG("%d %d\n", i, rv[i]);
        }
    }
}
