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


int main(int argc, char **argv) {
    LOG("test_drum.c\n");
    struct sequencer _s;
    struct sequencer *s = &_s;
    sequencer_init(s);
    s->task[0].tick = bd;
    s->task[1].tick = hh;

    struct {
        struct pattern p;
        struct pattern_step ps[3];
    } p = {
        .p = { .nb_steps = 3, },
        .ps = {
            {100, 12},
            {200,  8},
            {150,  8},
        },
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
