#include "macros.h"
#include <stdint.h>
void synth_tools_rs_init(void);
uint32_t test_synth_tools_rs_add1(uint32_t);
int main(int argc, char **argv) {
    synth_tools_rs_init();
    uint32_t x = 1;
    LOG("%d %d\n", x, test_synth_tools_rs_add1(x));
    return 0;
}
