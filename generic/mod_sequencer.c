/* Midi controlled drum machine (later, generic sequencer).
   Thinking to use Midi everywhere, then later generalize if necessary.
   Keep data structures free of allocation.  Should run on embedded.
   Timebase is MIDI clock, 24 ppqn (pulses per quarter note)
   Core of the implementation is the software timer from uc_tools.
*/
#include "swtimer.h"
#include <string.h>

/* Code is split up into two parts: a collection of tasks that can be
   resumed, and a software timer containing the next wake-up times of
   the tasks.  Tasks themselves are implemented as fat pointers. */
struct sequencer;
struct sequencer_task {
    /* Return values:
       0      Don't reschedule
       other  Reschedule in nb ticks
    */
    uint16_t (*tick)(struct sequencer *, void *);
    void *data;
};
struct sequencer {
    struct swtimer swtimer;
    struct swtimer_element swtimer_element[16];
    struct sequencer_task task[16];
};
void sequencer_init(struct sequencer *s) {
    memset(s,0,sizeof(*s));
    s->swtimer.arr = s->swtimer_element;
}
void sequencer_resume(struct sequencer *s, uint16_t task_nb) {
    // LOG("resume %d\n", task_nb);
    struct sequencer_task *t = &s->task[task_nb];
    uint16_t delta_time = t->tick(s, t->data);
    if (delta_time) {
        swtimer_schedule(&s->swtimer, delta_time, task_nb);
    }
}
void sequencer_tick(struct sequencer *s) {
    int logged = 0;
    for(;;) {
        if (s->swtimer.nb == 0) break;
        swtimer_element_t next = swtimer_peek(&s->swtimer);
        if (next.time_abs != s->swtimer.now_abs) break;
        if (!logged) {
            LOG("tick %d\n", s->swtimer.now_abs);
            logged = 1;
        }
        swtimer_pop(&s->swtimer);
        sequencer_resume(s, next.tag);
    }
    s->swtimer.now_abs++;
}
/* Convention: all tasks start at 0. */
void sequencer_start(struct sequencer *s) {
    for(uint16_t task_nb = 0; task_nb < ARRAY_SIZE(s->task); task_nb++) {
        if (s->task[task_nb].tick) {
            sequencer_resume(s, task_nb);
        }
    }
}
void sequencer_reset(struct sequencer *s) {
    swtimer_reset(&s->swtimer);
}


/* For patterns we use a sequential looping player that is driven by
   the software timer.  This avoids the need to dump all events in the
   timer at once. */

struct pattern_step {
    /* Do event, then wait for delay ticks. */
    uint16_t event;
    uint16_t delay;
};
struct pattern {
    uint16_t (*step_tick)(const struct pattern_step *step);
    uint16_t nb_steps;
    uint16_t next_step;
    const struct pattern_step *step;
};
uint16_t pattern_tick(struct sequencer *seq, void *data) {
    struct pattern *p = data;
    const struct pattern_step *step = &p->step[p->next_step++];
    if (p->next_step == p->nb_steps) {
        /* Loop */
        p->next_step = 0;
    }
    uint16_t delay = p->step_tick(step);
    LOG("pattern_tick delay %d\n", delay);
    return delay;
}
