#include "mod_sequencer.c"
#include "macros.h"

// The scheduler is a software timer playing back loops.

void pat_dispatch(struct sequencer *seq, const struct pattern_step *step) {
    LOG("pat_dispach %d %d\n", step->event.u16[1], step->delay);
}


int main(int argc, char **argv) {
    LOG("test_drum.c\n");
    struct sequencer _s,  *s  = &_s;  sequencer_init(s, pat_dispatch);

    pattern_t pat = 0;
    sequencer_add_step_cv(s, pat, 0, 100, 12);
    sequencer_add_step_cv(s, pat, 0, 200,  8);
    sequencer_add_step_cv(s, pat, 0, 150,  8);

    /* Convention: all tasks start at 0. */
    sequencer_start(s);

    for(int i=0;i<100;i++) {
        /* Called once per MIDI clock tick. */
        sequencer_tick(s);
    }
    return 0;
}
