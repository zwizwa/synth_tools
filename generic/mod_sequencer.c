/* Midi controlled drum machine (later, generic sequencer).
   Thinking to use Midi everywhere, then later generalize if necessary.
   Keep data structures free of allocation.  Should run on embedded.
   Timebase is MIDI clock, 24 ppqn (pulses per quarter note)
   Core of the implementation is the software timer from uc_tools.
*/
#include "swtimer.h"
#include <string.h>

/* Code is split up into two parts: a collection of loops (or finite
   sequences) represented as linked steps, and a software timer
   containing the next wake-up times of each loop. */


/* For patterns we use a sequential looping player that is driven by
   the software timer.  This avoids the need to dump all events in the
   timer at once. */

typedef uint16_t step_t;   // identifier for step event + link
typedef uint16_t dtime_t;  // delta-time in midi clocks

struct pattern_step {
    /* For now just use midi plus maybe some extensions.
       E.g. we know here that midi[0] contains the control code 80-FF,
       so 00-7F could be used for non-midi events. */
    union {
        uint8_t  u8[4];
        uint16_t u16[2];
        uint32_t u32;
    } event;
    /* Time delay to next event. */
    dtime_t delay;
    /* Index of next event in event pool. */
    step_t next;
};

/* E.g. PIXI CV uses 00-0F */
#include "uct_byteswap.h"
/* One of 16 MIDI ports with up to 3 (self-delimiting) MIDI bytes. */
#define PAT_MIDI_TAG(port) ((port) & 0x0F)
/* A 16-bit CV/Gate signal. */
#define PAT_CV_TAG   0xFF
#define PAT_CV(chan, val) {                     \
        .u8  = {                                \
            [0] = PAT_CV_TAG,                   \
            [1] = chan                          \
        },                                      \
       .u16 = {                                 \
            [1] = val                           \
       }                                        \
    }
#define PAT_MIDI(port, ...) {                           \
        .u8 = { PAT_MIDI_TAG(port), __VA_ARGS__ }       \
    }

/* Patterns are linked (circular) lists of steps that are stored in a
   pool, reusing the linking mechanism to represent a freelist. */
#define STEP_POOL_SIZE 128
#define STEP_NONE 0xFFFF
struct step_pool {
    struct pattern_step step[STEP_POOL_SIZE];
    step_t free;
};
static inline void step_pool_free(struct step_pool *p, step_t index) {
    p->step[index].next = p->free;
    p->free = index;
}
static inline uint16_t step_pool_alloc(struct step_pool *p) {
    uint16_t index = p->free;
    ASSERT(index != STEP_NONE); // out-of-memory
    p->free = p->step[index].next;
    p->step[index].next = STEP_NONE;
    return index;
}
static inline void step_pool_init(struct step_pool *p) {
    memset(p, 0, sizeof(*p));
    /* Initialize the free list. */
    p->free = STEP_NONE;
    for(int i=STEP_POOL_SIZE-1; i>=0; i--) {
        step_pool_free(p, i);
    }
}
/* It seems simplest to make constructors for each type. */
static inline uint16_t step_pool_new_cv(
    struct step_pool *sp, uint8_t chan, uint16_t val, dtime_t delay) {
    step_t i = step_pool_alloc(sp);
    struct pattern_step *p = sp->step + i;
    p->event.u8[0] = PAT_CV_TAG;
    p->event.u8[1] = chan;
    p->event.u16[1] = val;
    p->delay = delay;
    return i;
}

/* The size of the software timer is the number of simultaneous
   patterns that can be represented. ( Note that patterns of the same
   length could be merged. )*/
#define SEQUENCER_NB_PATTERNS 64

struct sequencer;
typedef void (*sequencer_fn)(struct sequencer *s, const struct pattern_step *p);

typedef uint16_t pattern_t;
struct sequencer {
    sequencer_fn dispatch;
    struct swtimer swtimer;
    struct swtimer_element swtimer_element[SEQUENCER_NB_PATTERNS];
    struct step_pool pool;
    /* Points to the last element in a sequence.
       We pick last so that start and insertion at end are both O(1) */
    step_t last[SEQUENCER_NB_PATTERNS];
};
void sequencer_init(struct sequencer *s, sequencer_fn dispatch) {
    memset(s,0,sizeof(*s));
    s->dispatch = dispatch;
    s->swtimer.arr = s->swtimer_element;
    step_pool_init(&s->pool);
    for(int i=0; i<SEQUENCER_NB_PATTERNS; i++) {
        s->last[i] = STEP_NONE;
    }
}
void sequencer_resume(struct sequencer *s, step_t step) {
    for(;;) {
        ASSERT(step != STEP_NONE);
        const struct pattern_step *p = &s->pool.step[step];
        s->dispatch(s, p);
        step = p->next;
        if (p->delay) {
            swtimer_schedule(&s->swtimer, p->delay, p->next);
            break;
        }
        else {
            /* Event has the same timestamp. */
            continue;
        }
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
    for(int i=0; i<SEQUENCER_NB_PATTERNS; i++) {
        step_t last_step = s->last[i];
        if (last_step != STEP_NONE) {
            step_t first_step = s->pool.step[last_step].next;
            ASSERT(first_step != STEP_NONE);
            swtimer_schedule(&s->swtimer, 0, first_step);
        }
        else {
            /* Pattern is empty. */
        }
    }
}
void sequencer_reset(struct sequencer *s) {
    swtimer_reset(&s->swtimer);
}

/* Add a step to an existing pattern, i.e. insert last element in the list. */
void sequencer_add_step_cv(struct sequencer *s, pattern_t pat_nb,
                           uint8_t chan, uint16_t val, dtime_t delay) {
    step_t step = step_pool_new_cv(&s->pool, chan, val, delay);

    step_t last = s->last[pat_nb];

    if (last == STEP_NONE) {
        /* If pattern is empty, create a loop of 1 step. */
        s->last[pat_nb] = step;
        s->pool.step[step].next = step;
        LOG("pat %d first step %d\n", pat_nb, step);
    }
    else {
        /* There is at least one step.  Add the new step at the end of
           the pattern. */
        step_t first = s->pool.step[last].next;
        s->pool.step[last].next = step;
        s->pool.step[step].next = first;
        s->last[pat_nb] = step;
        LOG("pat %d next step %d after %d\n", pat_nb, step, last);
    }
}



