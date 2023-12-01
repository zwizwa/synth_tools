
/* Reduce the size to make tests easier to analyze. */
#define STEP_POOL_SIZE 10
#define PATTERN_POOL_SIZE 10

/* Works up to size 64 */
#define STEP_ALL_FREE ((1<<STEP_POOL_SIZE)-1)
#define PATTERN_ALL_FREE ((1<<PATTERN_POOL_SIZE)-1)

#include "mod_sequencer.c"
#include "macros.h"

// The scheduler is a software timer playing back loops.

/* Note that dispatch does not know about the step delay to the
   following event: that is considered sequencer implementation. */
void pat_dispatch(struct sequencer *seq, const union pattern_event *ev) {
    LOG("%4d pat_dispach %d\n",
        seq->time,
        ev->u16[1]);
}

pattern_t test_pattern_1(struct sequencer *s) {
    LOG("alloc pat1\n");
    pattern_t pat = sequencer_pattern_alloc(s);
    sequencer_add_step_cv(s, pat, 0, 100, 12);
    sequencer_add_step_cv(s, pat, 0, 200,  8);
    sequencer_add_step_cv(s, pat, 0, 150,  8);
    swtimer_schedule(&s->swtimer, 0, pat);
    sequencer_info_pattern(s, pat);
    return pat;
}
pattern_t test_pattern_2(struct sequencer *s) {
    LOG("alloc pat2\n");
    pattern_t pat = sequencer_pattern_alloc(s);
    sequencer_add_step_cv(s, pat, 0, 1001, 4);
    swtimer_schedule(&s->swtimer, 0, pat);
    sequencer_info_pattern(s, pat);
    return pat;
}

pattern_t test_pattern_3(struct sequencer *s) {
    LOG("alloc pat3\n");
    pattern_t pat = sequencer_pattern_alloc(s);
    sequencer_add_step_cv(s, pat, 0, 1002, 8);
    swtimer_schedule(&s->swtimer, 0, pat);
    sequencer_info_pattern(s, pat);
    return pat;
}

void test_pool_and_play(struct sequencer *s) {
    ASSERT(s->pattern_pool.free != PATTERN_NONE);

    pattern_t pat1 = test_pattern_1(s);
    pattern_t pat2 = test_pattern_2(s);
    pattern_t pat3 = test_pattern_3(s);
    LOG("pats %d %d %d\n", pat1, pat2, pat3);

    for(int i=0;i<100;i++) {
        /* Called once per MIDI clock tick. */
        sequencer_tick(s);
    }

    LOG("end of first run.  now dropping patterns\n");

    pattern_pool_info(&s->pattern_pool);
    step_pool_info(&s->step_pool);

    // Restart and run for a bit.
    sequencer_restart(s);
    for(int i=0;i<100;i++) {
        sequencer_tick(s);
    }



    // This will delete the step cycle, but the pattern slot is not yet freed.
    sequencer_drop_pattern(s, pat1);
    sequencer_drop_pattern(s, pat2);
    sequencer_drop_pattern(s, pat3);

    pattern_pool_info(&s->pattern_pool);
    step_pool_info(&s->step_pool);


    // Already allocate the new patterns
    pattern_t npat1 = test_pattern_1(s);
    pattern_t npat2 = test_pattern_2(s);
    pattern_t npat3 = test_pattern_3(s);
    LOG("npats %d %d %d\n", npat1, npat2, npat3);


    // Run the sequencer for a couple of steps to flush out the
    // patterns while sequencing the new.
    for(int i=0;i<100;i++) {
        sequencer_tick(s);
    }

    sequencer_drop_pattern(s, npat1);
    sequencer_drop_pattern(s, npat2);
    sequencer_drop_pattern(s, npat3);
    // Flush delete
    for(int i=0;i<100;i++) {
        sequencer_tick(s);
    }

    ASSERT(PATTERN_ALL_FREE == pattern_pool_info(&s->pattern_pool));
    ASSERT(STEP_ALL_FREE == step_pool_info(&s->step_pool));

    // Completely reloading might be simpler. That will be O(NlogN).
    // Probably possible to init the heap in O(N) as well.
}
void test_record(struct sequencer *s) {
    // Full library call sequence.
    // - track 0, establish main beat
    // - record additional tracks

    sequencer_cursor_open(s, 24 * 2);
    union pattern_event ev = {.u8 = {1,2,3,4}};
    sequencer_cursor_write(s, &ev);
    sequencer_ntick(s, 12);
    sequencer_cursor_write(s, &ev);
    sequencer_ntick(s, 24);
    sequencer_cursor_write(s, &ev);
    sequencer_info_pattern(s, s->cursor.pattern);

    // Pattern should start playing at t = 24 * 2
    sequencer_ntick(s, 12 + 2 * 24 * 2);


}

int main(int argc, char **argv) {
    LOG("test_drum.c\n");
    struct sequencer _s, *s  = &_s;
    sequencer_init(s, pat_dispatch);
    s->verbose = 1;
    //test_pool_and_play(s);
    test_record(s);
    return 0;
}
