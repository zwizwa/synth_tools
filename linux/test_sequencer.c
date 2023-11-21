
/* Reduce the size to make tests easier to analyze. */
#define STEP_POOL_SIZE 16
#define PATTERN_POOL_SIZE 16

#include "mod_sequencer.c"
#include "macros.h"

// The scheduler is a software timer playing back loops.

void pat_dispatch(struct sequencer *seq, const struct pattern_step *step) {
    LOG("pat_dispach %d %d\n", step->event.as.u16[1], step->delay);
}

pattern_t test_pattern_1(struct sequencer *s) {
    LOG("alloc pat1\n");
    pattern_t pat = pattern_pool_alloc(&s->pattern_pool);
    sequencer_add_step_cv(s, pat, 0, 100, 12);
    sequencer_add_step_cv(s, pat, 0, 200,  8);
    sequencer_add_step_cv(s, pat, 0, 150,  8);
    return pat;
}
pattern_t test_pattern_2(struct sequencer *s) {
    LOG("alloc pat2\n");
    pattern_t pat = pattern_pool_alloc(&s->pattern_pool);
    sequencer_add_step_cv(s, pat, 0, 1001, 4);
    return pat;
}

pattern_t test_pattern_3(struct sequencer *s) {
    LOG("alloc pat3\n");
    pattern_t pat = pattern_pool_alloc(&s->pattern_pool);
    sequencer_add_step_cv(s, pat, 0, 1002, 8);
    return pat;
}

int main(int argc, char **argv) {
    LOG("test_drum.c\n");
    struct sequencer _s,  *s  = &_s;
    sequencer_init(s, pat_dispatch);
    ASSERT(s->pattern_pool.free != PATTERN_NONE);

    pattern_t pat1 = test_pattern_1(s);
    pattern_t pat2 = test_pattern_2(s);
    pattern_t pat3 = test_pattern_3(s);
    LOG("pats %d %d %d\n", pat1, pat2, pat3);

    sequencer_start(s);
    for(int i=0;i<100;i++) {
        /* Called once per MIDI clock tick. */
        sequencer_tick(s);
    }

    LOG("end of first run.  now dropping patterns\n");

    // This will delete the step cycle, but the pattern slot is not yet freed.
    sequencer_drop_pattern(s, pat1);
    sequencer_drop_pattern(s, pat2);
    sequencer_drop_pattern(s, pat3);

    // Run the sequencer for a couple of steps to flush out the patterns.
    for(int i=0;i<100;i++) {
        sequencer_tick(s);
    }

    pattern_pool_info(&s->pattern_pool);
    step_pool_info(&s->step_pool);


    return 0;
}
