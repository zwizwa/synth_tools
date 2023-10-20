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

#if 0
    struct pattern p = {
        {0, 100},
        {1, 200},
        {3, 150},
    };
    spawn_pattern(s, &p, ARRAY_SIZE(p),
                  4 /* Loop point */,
                  quarter_notes /* timescale */);
#endif

    /* Convention: all tasks start at 0. */
    sequencer_start(s);


    for(int i=0;i<100;i++) {
        /* Called once per MIDI clock tick. */
        sequencer_tick(s);
    }
    return 0;
}
