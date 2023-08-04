#include "mod_drum.c"
#include "macros.h"
int main(int argc, char **argv) {
    LOG("test_drum.c\n");
    struct drum _s = {};
    struct drum *s = &_s;
    drum_init(s);
    s->event[0].period = 10;
    drum_dispatch(s, 0);
    for(int i=0;i<1000;i++) {
        drum_tick(s);
    }
    return 0;
}
