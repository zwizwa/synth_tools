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
    pat++;
    sequencer_add_step_cv(s, pat, 0, 1001, 4);
    pat++;
    sequencer_add_step_cv(s, pat, 0, 1002, 8);

    /* Convention: all tasks start at 0. */
    sequencer_start(s);

    for(int i=0;i<100;i++) {
        /* Called once per MIDI clock tick. */
        sequencer_tick(s);
    }

    sequencer_drop_pattern(s, 0);  // 3 events
    sequencer_drop_pattern(s, 1);  // 1 event
    sequencer_drop_pattern(s, 2);  // 1 event
    sequencer_drop_pattern(s, 3);  // alread empty

    return 0;
}
