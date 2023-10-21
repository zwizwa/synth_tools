#include "mod_sequencer.c"
#include "macros.h"

// NEXT: "spawn a sequence"

// Then map a CC to set the value of the sequence

// To not put too much pressure on the timer storage, only store one
// next event.  Each sequence is a state machine scheduling the next
// event.

uint16_t bd(struct sequencer *s, void *no_state) {
    LOG("bd\n");
    return 24;
}
uint16_t hh(struct sequencer *s, void *no_state) {
    LOG("hh\n");
    return 12;
}

uint16_t pat_tick(const struct pattern_step *step) {
    LOG("pat_tick %d %d\n", step->event, step->delay);
    return step->delay;
}


int main(int argc, char **argv) {
    LOG("test_drum.c\n");
    struct sequencer _s;
    struct sequencer *s = &_s;
    sequencer_init(s);
    s->task[0].tick = bd;
    s->task[1].tick = hh;

    struct pattern_step ps[] = {
        {100, 12},
        {200,  8},
        {150,  8},
    };
    struct pattern p = {
        .step_tick = pat_tick,
        .nb_steps = ARRAY_SIZE(ps),
        .step = ps,
    };
    s->task[2].tick = pattern_tick;
    s->task[2].data = &p;

    /* Convention: all tasks start at 0. */
    sequencer_start(s);


    for(int i=0;i<100;i++) {
        /* Called once per MIDI clock tick. */
        sequencer_tick(s);
    }
    return 0;
}
