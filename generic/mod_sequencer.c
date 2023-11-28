/* Midi controlled drum machine (later, generic sequencer).
   Thinking to use Midi everywhere, then later generalize if necessary.
   Keep data structures free of allocation.  Should run on embedded.
   Timebase is MIDI clock, 24 ppqn (pulses per quarter note)
   Core of the implementation is the software timer from uc_tools.
*/

/* About recording:
   Currently there are two implementations.

   1. A live recorder: start with empty fixed loop and add events
      on-the-fly.  Play immediately.

   2. A bootstrap recorder: time-stamp events, send to Erlang, let it
      perform a computation to snap to midi grid and obtain tempo, and
      let it push that data to the sequencer and start.

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

struct pattern_event {
    /* For now just use midi plus maybe some extensions.
       E.g. we know here that midi[0] contains the control code 80-FF,
       so 00-7F could be used for non-midi events. */
    union {
        uint8_t  u8[4];
        uint16_t u16[2];
        uint32_t u32;
    } as;
};

#define STEP_DELAY_NONE 0xFFFF

struct pattern_step {
    struct pattern_event event;
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
#define PAT_CV_TAG   0xFE
/* Internal sequencer bookkeeping. */
#define PAT_SEQ_CMD  0xFF
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

#ifndef STEP_POOL_SIZE
#define STEP_POOL_SIZE 128
#endif


#define STEP_NONE 0xFFFF
#define STEP_DEAD 0xFFFE

struct step_pool {
    struct pattern_step step[STEP_POOL_SIZE];
    step_t free;
};
static inline void step_pool_free(struct step_pool *p, step_t index) {
    p->step[index].next = p->free;
    p->free = index;
}
/* Break loop, push it to the freelist. */
static inline void step_pool_free_loop(struct step_pool *p, step_t last) {
    struct pattern_step *plast = &p->step[last];
    step_t first = plast->next;
    plast->next = p->free;
    p->free = first;
}
static inline uint64_t step_pool_info(struct step_pool *p) {
    uint64_t mask = 0;
    LOG("step free:");
    for(step_t i=p->free; i != STEP_NONE; i=p->step[i].next) {
        LOG(" %d", i);
        mask |= (1 << i);
    }
    LOG("\n");
    return mask;
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
static inline uint16_t step_pool_new_event(
    struct step_pool *sp, const struct pattern_event *ev, dtime_t delay) {
    step_t i = step_pool_alloc(sp);
    struct pattern_step *p = sp->step + i;
    p->event = *ev;
    p->delay = delay;
    return i;
}


struct pattern_phase {
    /* Points to the current play head in a sequence. This indirection
       (software timer stores just pattern number) makes it simpler to
       change patterns on the fly without O(N) operation on the
       timer. */
    /* In the freelist, this doubles as list link. */
    step_t head;
    /* Points to the last element in a sequence.  We pick last here
       and not first, so that start and insertion at end are both
       O(1) operations */
    step_t last;
};

/* The representation got a bit confusing, so make a separate
   function.  A pattern_phase struct can be in one of 3 lifecycle
   states: not allocated, allocated and used, allocated and waiting
   for deletion. */
enum pattern_phase_lifecycle {
    pattern_phase_unused = 0,
    pattern_phase_used = 1,
    pattern_phase_dead = 2,
};
static inline enum pattern_phase_lifecycle pattern_phase_lifecycle(struct pattern_phase *pp) {
    if (pp->last == STEP_NONE) return pattern_phase_unused;
    if (pp->last == STEP_DEAD) return pattern_phase_dead;
    return pattern_phase_used;
}


/* The size of the software timer is the number of simultaneous
   patterns that can be represented. ( Note that patterns of the same
   length could be merged. )*/
#ifndef PATTERN_POOL_SIZE
#define PATTERN_POOL_SIZE 64
#endif
typedef uint16_t pattern_t;
#define PATTERN_NONE 0xffff
struct pattern_pool {
    struct pattern_phase pattern[PATTERN_POOL_SIZE];
    pattern_t free;
};


static inline void pattern_pool_free(struct pattern_pool *p, pattern_t index) {
    /* Note that in the freelist, the head pointer is used to link the
       patterns together. */
    p->pattern[index].head = p->free;
    /* See pattern_phase_lifecycle() */
    p->pattern[index].last = STEP_NONE;
    p->free = index;
}
static inline pattern_t pattern_pool_alloc(struct pattern_pool *p) {
    LOG("pattern_pool_alloc p->free = 0x%x\n", p->free);
    uint16_t index = p->free;
    ASSERT(index != PATTERN_NONE); // out-of-memory
    p->free = p->pattern[index].head;
    p->pattern[index].head = STEP_NONE;
    return index;
}
static inline uint64_t pattern_pool_info(struct pattern_pool *p) {
    uint64_t mask = 0;
    LOG("patn free:");
    for(pattern_t i=p->free; i != PATTERN_NONE; i=p->pattern[i].head) {
        LOG(" %d", i);
        mask |= (1 << i);
    }
    LOG("\n");
    /* Membership bitmask for testing. */
    return mask;
}
static inline void pattern_pool_init(struct pattern_pool *p) {
    memset(p, 0, sizeof(*p));
    /* Initialize the free list. */
    p->free = PATTERN_NONE;
    for(int i=PATTERN_POOL_SIZE-1; i>=0; i--) {
        pattern_pool_free(p, i);
    }
    //pattern_pool_info(p);
}


struct sequencer;
typedef void (*sequencer_fn)(struct sequencer *s, const struct pattern_step *p);

struct sequencer_cursor {
    /* Pattern we are recording to. */
    pattern_t pattern;
    /* Used for live recording: number of MIDI clocks since last
       recorded event. */
    dtime_t delay;
};
struct sequencer_transaction {
    pattern_t pattern;
};

struct sequencer {
    sequencer_fn dispatch;
    uintptr_t time;
    struct sequencer_cursor cursor;
    struct sequencer_transaction transaction;
    struct swtimer swtimer;
    struct swtimer_element swtimer_element[PATTERN_POOL_SIZE];
    struct step_pool step_pool;
    struct pattern_pool pattern_pool;
    uint8_t verbose:1;
};
struct pattern_phase *sequencer_pattern(struct sequencer *s, pattern_t nb) {
    ASSERT(nb < PATTERN_POOL_SIZE);
    return &s->pattern_pool.pattern[nb];
}
struct pattern_step *sequencer_step(struct sequencer *s, step_t nb) {
    ASSERT(nb < STEP_POOL_SIZE);
    return &s->step_pool.step[nb];
}
void sequencer_init(struct sequencer *s, sequencer_fn dispatch) {
    memset(s,0,sizeof(*s));
    s->cursor.pattern = PATTERN_NONE;
    s->transaction.pattern = PATTERN_NONE;
    s->dispatch = dispatch;
    s->swtimer.arr = s->swtimer_element;
    step_pool_init(&s->step_pool);
    pattern_pool_init(&s->pattern_pool);
}
void sequencer_tick(struct sequencer *s) {
    int logged = 0;
    for(;;) {
        if (s->swtimer.nb == 0) {
            // LOG("no more swtimer events\n");
            break;
        }
        swtimer_element_t next = swtimer_peek(&s->swtimer);
        if (next.time_abs != s->swtimer.now_abs) break;
        if (!logged) {
            if (s->verbose) { LOG("tick time=%d\n", s->swtimer.now_abs); };
            logged = 1;
        }
        swtimer_pop(&s->swtimer);
        /* The tag refers to the pattern number, which can be obtained
           to store the next step in the sequence. */
        uintptr_t pattern_nb = next.tag;
        if (s->verbose) { LOG("pattern %d\n", pattern_nb); }
        ASSERT(pattern_nb < PATTERN_POOL_SIZE);
        struct pattern_phase *pp = sequencer_pattern(s, pattern_nb);
        step_t step = pp->head;

        switch(pattern_phase_lifecycle(pp)) {
        case pattern_phase_dead:
            /* This pattern is an empty shell left over by
               sequencer_drop_pattern(). We collect it here */
            ASSERT(pp->last == STEP_DEAD);
            LOG("collecting pattern_nb %d\n", pattern_nb);
            pattern_pool_free(&s->pattern_pool, pattern_nb);
            break;
        case pattern_phase_used:
            /* Dispatch all events in this pattern that happen at this
               time instance. */
            for(;;) {
                if (s->verbose) { LOG("step %d\n", step); }
                const struct pattern_step *p = sequencer_step(s, step);
                s->dispatch(s, p);
                if (p->delay > 0) {
                    /* Next event is in the future. */
                    pp->head = p->next;
                    swtimer_schedule(&s->swtimer, p->delay, pattern_nb);
                    break;
                }
                else {
                    /* Next event has the same timestamp. */
                    ASSERT(step != p->next);
                    step = p->next;
                    continue;
                }
            }
            break;
        case pattern_phase_unused:
            ERROR("unused pattern found in timer heap\n");
            break;
        }
    }
    /* Software timer uses 16 bit circular time, which at 120bpm is
       about 22 minutes.  This is the maximum time between steps, and
       the maximum total time of a live recorded pattern. */
    s->swtimer.now_abs++;
    /* Accumulate the inter-step delay for live recording.*/
    s->cursor.delay++;
    /* Keep track of global time.  This is for debugging only.  The 32
       bits can accomodate a bit less than 3 years at 120bpm. */
    s->time++;
}
void sequencer_ntick(struct sequencer *s, uintptr_t n) {
    while(n--) sequencer_tick(s);
}

/* Clear timer and restart all loops from the beginning. */
// FIXME: Can be used for full (offline) reload as well.
void sequencer_restart(struct sequencer *s) {

    swtimer_reset(&s->swtimer);
    for(int pattern_nb=0; pattern_nb<PATTERN_POOL_SIZE; pattern_nb++) {
        struct pattern_phase *pp = sequencer_pattern(s, pattern_nb);
        step_t last_step = pp->last;
        switch(pattern_phase_lifecycle(pp)) {
        case pattern_phase_dead:
            LOG("collecting pattern_nb %d\n", pattern_nb);
            pattern_pool_free(&s->pattern_pool, pattern_nb);
            break;
        case pattern_phase_used: {
            struct pattern_step *plast = sequencer_step(s, last_step);
            step_t first_step = plast->next;
            ASSERT(first_step != STEP_NONE);
            pp->head = first_step;
            swtimer_schedule(&s->swtimer, 0, pattern_nb);
            break;
            }
        case pattern_phase_unused:
            break;
        }
    }
}

/* Expose internal structure as iterators.  Originally written to
   create state dump functionality. */
void sequencer_foreach_pattern(struct sequencer *s,
                               void (*visit)(struct sequencer *s,
                                             void *state, pattern_t),
                               void *state) {
    for(int pattern_nb=0; pattern_nb<PATTERN_POOL_SIZE; pattern_nb++) {
        struct pattern_phase *pp = sequencer_pattern(s, pattern_nb);
        switch(pattern_phase_lifecycle(pp)) {
        case pattern_phase_used:
            visit(s, state, pattern_nb);
            break;
        default:
            break;
        }
    }
}
void sequencer_foreach_step(struct sequencer *s,
                            pattern_t pattern_nb,
                            void (*visit)(struct sequencer *s,
                                          void *state, struct pattern_step *),
                            void *state) {
    struct pattern_phase *pp = sequencer_pattern(s, pattern_nb);
    step_t last_step = pp->last;
    ASSERT(last_step != STEP_NONE);
    struct pattern_step *plast = sequencer_step(s, last_step);
    step_t first_step = plast->next;
    ASSERT(first_step != STEP_NONE);
    step_t step = first_step;
    for(;;) {
        struct pattern_step *pstep = sequencer_step(s, step);
        visit(s, state, pstep);
        if (step == last_step) break;
        step = pstep->next;
    }
}
struct sequencer_dump {
};
void sequencer_dump_step(struct sequencer *s, void *_dump, struct pattern_step *ps) {
}
void sequencer_dump_pattern(struct sequencer *s, void *_dump, pattern_t pattern_nb) {
    sequencer_foreach_step(s, pattern_nb, sequencer_dump_step, _dump);
}
void sequencer_dump(struct sequencer *s) {
    struct sequencer_dump dump = {};
    sequencer_foreach_pattern(s, sequencer_dump_pattern, &dump);
}


/* FIXME: Create a function that checks the main invariants:
   - the timer heap should not contain duplicate references
   - each allocated pattern should be referenced in the timer heap
   - a pattern can be empty, which indicates it should be deleted */
void sequencer_check_invariants(struct sequencer *s) {
}


/* Add a step to an existing pattern, i.e. insert last element in the list. */
void sequencer_add_step_event(struct sequencer *s, pattern_t pat_nb,
                              const struct pattern_event *ev, dtime_t delay) {
    step_t step = step_pool_new_event(&s->step_pool, ev, delay);
    struct pattern_step *pstep = sequencer_step(s, step);
    struct pattern_phase *pp = sequencer_pattern(s, pat_nb);
    step_t last = pp->last;

    if (last == STEP_NONE) {
        /* If pattern is empty, create a loop of 1 step. */
        pstep->next = step;
        /* Link it into the current pattern. */
        pp->last = step;
        pp->head = step;
        LOG("pat %d first step %d\n", pat_nb, step);
    }
    else {
        /* There is at least one step.  Add the new step at the end of
           the pattern. */
        struct pattern_step *plast = sequencer_step(s, last);
        step_t first = plast->next;
        pstep->next = first; // new step followed by first
        plast->next = step; // last step followed by new step
        pp->last = step; // new step is now last step
        LOG("pat %d next step %d after %d\n", pat_nb, step, last);
    }
}
void sequencer_add_step_cv(struct sequencer *s, pattern_t pat_nb,
                           uint8_t chan, uint16_t val, dtime_t delay) {
    struct pattern_event ev = {};
    ev.as.u8[0] = PAT_CV_TAG;
    ev.as.u8[1] = chan;
    ev.as.u16[1] = val;
    sequencer_add_step_event(s, pat_nb, &ev, delay);
}



/* Note that the timer heap still contains a reference to the pattern.
   We can't easily remove that so the timer event is allowed to
   expire, which will free the pattern slot at that time.  We can
   however already delete the step cycle. */
void sequencer_drop_pattern(struct sequencer *s, pattern_t pat_nb) {
    ASSERT(pat_nb < PATTERN_POOL_SIZE);
    step_t last = s->pattern_pool.pattern[pat_nb].last;
    if (last == STEP_NONE) {
        LOG("Pattern %d is empty\n", pat_nb);
    }
    else {
        LOG("Pattern %d, removing cycle at %d\n", pat_nb, last);
        step_pool_free_loop(&s->step_pool, last);
        struct pattern_phase *pp = sequencer_pattern(s, pat_nb);
        pp->head = STEP_DEAD;
        pp->last = STEP_DEAD;
    }
}

void sequencer_info_pattern(struct sequencer *s, pattern_t pat_nb) {
    LOG("pattern %d:\n", pat_nb);
    struct pattern_phase *pp = sequencer_pattern(s, pat_nb);
    struct pattern_step *ps_last = sequencer_step(s, pp->last);
    step_t p = ps_last->next;
    for(;;) {
        struct pattern_step *ps = sequencer_step(s, p);
        LOG("  step %d:", p);
        for (int i=0; i<4; i++) LOG(" %02x", ps->event.as.u8[i]);
        LOG(" delay %d\n", ps->delay);
        if (p == pp->last) { break; }
        p = ps->next;
    }
}


/* Live recording */

/* Incremental recording requires a special data structure.  The
   cursor consists of 2 parts: the event data is written in the new
   step, while the existing old step delay is split between old and
   new step. */
pattern_t sequencer_cursor_open(struct sequencer *s, dtime_t duration) {
    struct sequencer_cursor *c = &s->cursor;
    ASSERT(c->pattern == PATTERN_NONE);
    c->delay = 0;
    pattern_t pat = pattern_pool_alloc(&s->pattern_pool);
    c->pattern = pat;
    // FIXME: This should be an event that performs some cleanup,
    // e.g. remove the pattern from the cursor.
    struct pattern_event ev = {};
    sequencer_add_step_event(s, pat, &ev, duration);
    swtimer_schedule(&s->swtimer, duration, pat);
    return pat;
}
void sequencer_cursor_write(struct sequencer *s, const struct pattern_event *ev) {
    struct sequencer_cursor *c = &s->cursor;
    struct pattern_phase *pp = sequencer_pattern(s, c->pattern);
    struct pattern_step *last = sequencer_step(s, pp->last);
    dtime_t time_left = last->delay - c->delay;
    last->delay = c->delay;
    c->delay = 0;
    sequencer_add_step_event(s, c->pattern, ev, time_left);
}


/* Command transactions. */
pattern_t sequencer_pattern_begin(struct sequencer *s, dtime_t nb_clocks) {
    ASSERT(s->transaction.pattern == PATTERN_NONE);
    ASSERT(nb_clocks > 0);

    pattern_t pat_nb = pattern_pool_alloc(&s->pattern_pool);
    LOG("alloc pattern nb = %d\n", pat_nb);
    /* Caller should be aware of time scale, and should be able to
       fill at least the first event before the pattern is
       scheduled, otherwise an ASSERT will fail (FIXME). */
    swtimer_schedule(&s->swtimer, nb_clocks, pat_nb);
    /* Note that the cursor behaves as a weak pointer.  The strong
       pointer is the reference inside the timer heap. */
    s->transaction.pattern = pat_nb;
    /* Also return a reference to the pattern. */
    return pat_nb;
}
void sequencer_pattern_end(struct sequencer *s) {
    ASSERT(s->transaction.pattern != PATTERN_NONE);
    /* Note that the cursor behaves as a weak pointer.  The strong
       pointer is the reference inside the timer heap. */
    LOG("pattern_end %d\n", s->transaction.pattern);
    sequencer_info_pattern(s, s->transaction.pattern);
    s->transaction.pattern = PATTERN_NONE;
}
void sequencer_pattern_step(struct sequencer *s, const struct pattern_event *ev, dtime_t delay) {
    ASSERT(s->transaction.pattern != PATTERN_NONE);
    sequencer_add_step_event(s, s->transaction.pattern, ev, delay);
}

