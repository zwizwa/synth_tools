/* Midi controlled drum machine.
   Thinking to use Midi everywhere, then later generalize if necessary.
   Keep data structures free of allocation.  Should run on embedded.
   Timebase is MIDI clock, 24 ppqn (pulses per quarter note)
   Core of the implementation is the software timer from uc_tools.
*/
#include "swtimer.h"
#include <string.h>

struct event {
    uint16_t period;
};
struct drum {
    struct swtimer swtimer;
    struct swtimer_element swtimer_element[16];
    struct event event[16];
};
void drum_init(struct drum *s) {
    memset(s,0,sizeof(*s));
    s->swtimer.arr = s->swtimer_element;
}
void drum_dispatch(struct drum *s, uint16_t tag) {
    LOG("%d\n", tag);
    struct event *e = &s->event[tag];
    if (e->period) {
        swtimer_schedule(&s->swtimer, e->period, tag);
    }
}
void drum_tick(struct drum *s) {
    LOG("now %d\n", s->swtimer.now_abs);
    for(;;) {
        if (s->swtimer.nb == 0) break;
        swtimer_element_t next = swtimer_peek(&s->swtimer);
        if (next.time_abs != s->swtimer.now_abs) break;
        swtimer_pop(&s->swtimer);
        drum_dispatch(s, next.tag);
    }
    s->swtimer.now_abs++;
}

