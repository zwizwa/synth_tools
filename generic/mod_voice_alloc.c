#ifndef MOD_VOICE_ALLOC
#define MOD_VOICE_ALLOC


/* This is much more tricky than I originally anticipated.

   Part of the problem is also that MIDI is a real-time protocol and
   it would really help to know note durations in advance.  The
   sequencers do essentially have that information but in order to use
   that it would make the implementation essentially non-real-time.
   So two implementations would be needed: one real time, and one
   offline that is more optimal but needs information about the future.

   We don't do that.  Build something that is primarily real-time and
   use the same information for the sequencer-driven setup.  Note that
   the current sequencer implementation also does not use durations,
   but separate note on and off events.

   There are many ways to specify this problem, so be very explicit
   about what we want to do


   Requirements:

   - LRU voice allocation: reuse the oldest OFF note, then use the
     oldes ON note.

   - Piano model: only one simultaneous voice per key.  This is
     closest to MIDI, which does not distinguish separate instances of
     the same key.

   Later, define some other models:

   - For monophonic operation I want stack-like behavior, where new
     notes override older ones.  This is not really relevant here.  A
     separate allocator will need to be built for that.

   - Duration-based allocation

   - Non-piano / non-MIDI model: allow for multiple instances of the
     same note by adding tags.

   This code is tricky enough to merit a unit test.  See
   test_voice_alloc.c

*/


/* Data structures: We essentially want queues (FIFOs) with element
   removal.  E.g. two queues, note)on and note_off.  When an on/off
   event happens it is written into the queue, _AND_ removed from the
   other queue.  The removal operation seems to require the need of a
   doubly linked list. Since elements can only appear in one of the
   two queues we can simplify the implementation by associating a
   next/prev pointer to each of the voices.  Also, the pointers
   themselves will only point to other notes so can be represented as
   bytes.  Space-efficient representation might be necessary for
   implementation on uC or FPGA, so let's do it from the start.  Since
   it is a doubly linked list we can choose the orientation: the first
   element in the list is the one to be allocated next.  Freshly freed
   elements are moved to the back. */


#include <stdint.h>
#include <string.h>
#include "macros.h"

#define VOICE_NONE 255
#define NOTE_NONE  255


#ifndef NB_VOICES
#define NB_VOICES 64
#endif

#ifndef NB_NOTES
#define NB_NOTES 128
#endif

#ifndef VOICE_ALLOC_LOG
#define VOICE_ALLOC_LOG LOG
#endif

/* To keep the implementation simple, the linked lists are implemented
   as circular bi-directional lists with a marker-only head element
   that sits in between start and end.  These heads are added to the
   array of voices. */
#define VOICE_OFF (NB_VOICES)
#define VOICE_ON  (NB_VOICES+1)

#ifndef VOICE_MAX_SEMA
#define VOICE_MAX_SEMA 255
#endif

struct voice_meta {
    uint8_t prev;  // previous element in on or off queue
    uint8_t next;  // next ...
    uint8_t sema;  // 0=off, >0 nb_on
    uint8_t note;  // voice->note map
};


struct voice_alloc {
    struct voice_meta voice[NB_VOICES + 2 /* VOICE_OFF, VOICE_ON */];
    uint8_t note_to_voice[NB_NOTES];
};

/* Implement insert + remove primitives. */

/* Precondition: voice is not part of any list.  This is necessary
   because the next/prev pointers are stored in the voice struct, not
   separately. */
static inline void voice_alloc_insert_before(
    struct voice_alloc *va,
    uint8_t element_nb,
    uint8_t new_before_nb) {

    /* Standard circular list insert before element.  Three elements
       need to be updated: the element marker, the new before element,
       and the previous before element.

       PRE:      [prev_before] <-> [element]
       POST:     [prev_before] <-> [new_before] <-> [element]

     */

    struct voice_meta *element     = &va->voice[element_nb];
    struct voice_meta *prev_before = &va->voice[element->prev];
    struct voice_meta *new_before  = &va->voice[new_before_nb];

    ASSERT(new_before->prev == VOICE_NONE);
    ASSERT(new_before->next == VOICE_NONE);

    new_before->next  = element_nb;
    new_before->prev  = element->prev;
    prev_before->next = new_before_nb;
    element->prev     = new_before_nb;

}

/* After this, the element needs to be added to the other list to
   maintain the invariant that the voice is in one of the two
   lists. */
static inline void voice_alloc_remove(
    struct voice_alloc *va,
    uint8_t to_remove_nb) {

    /* Standard circular list remove.  Three elements need to be
       updated.  The element to be removed, and the elements before
       and after (which can include a head/tail marker.

       PRE:     [before] <-> [to_remove] <-> [after]
       POST:    [before] <-> [after]
    */

    struct voice_meta *to_remove = &va->voice[to_remove_nb];
    struct voice_meta *before    = &va->voice[to_remove->prev];
    struct voice_meta *after     = &va->voice[to_remove->next];

    before->next = to_remove->next;
    after->prev  = to_remove->prev;

    to_remove->next = VOICE_NONE;
    to_remove->prev = VOICE_NONE;
}



/* Implement the allocator primitives directly in terms of the
   insert/remove primitives.  No need to make this implementation very
   layered. */


static inline int voice_alloc_queue_empty(struct voice_alloc *va, uint8_t queue_nb) {
    struct voice_meta *q = &va->voice[queue_nb];
    if (q->next == queue_nb) {
        ASSERT(q->prev == queue_nb);
        return 1;
    }
    else {
        return 0;
    }
}

void voice_alloc_note_on(struct voice_alloc *va, uint8_t note) {
    ASSERT(note < NB_NOTES);
    uint8_t voice_nb = va->note_to_voice[note];
    if (voice_nb == VOICE_NONE) {
        /* Note is not associated to a voice.  Identify which voice to
           reuse. One of the queues is guaranteed to be non-empty. */
        if (!voice_alloc_queue_empty(va, VOICE_OFF)) {
            voice_nb = va->voice[VOICE_OFF].next;
        }
        else {
            ASSERT(!voice_alloc_queue_empty(va, VOICE_ON));
            voice_nb = va->voice[VOICE_ON].next;
        }
        /* Remove it from the queue it was in and move it to the back
           of the ON queue.  Circular list, so this is implemented as
           before the beginning. */
        voice_alloc_remove(va, voice_nb);
        voice_alloc_insert_before(va, VOICE_ON, voice_nb);

        /* Re-initialize, killing all traces of the old voice. */
        struct voice_meta *v = &va->voice[voice_nb];
        uint8_t old_note = v->note;
        if (old_note != NOTE_NONE) {
            va->note_to_voice[old_note] = VOICE_NONE;
        }
        v->sema = 1;
        v->note = note;
        va->note_to_voice[note] = voice_nb;
        // FIXME: note_to_inc
        // FIXME: trigger the envelope
    }
    else {
        /* Note is already associated to a voice in on or off state
           (decaying).  This means we can simply reuse it. */
        struct voice_meta *v = &va->voice[voice_nb];

        /* Check that back reference is consistent. */
        ASSERT(note == v->note);

        if (0 == v->sema) {
            /* Voice is in the off state, we will reactivate it.  Move
               it from the off queue into the end of the on queue. */
            voice_alloc_remove(va, voice_nb);
            voice_alloc_insert_before(va, VOICE_ON, voice_nb);
            // FIXME: trigger the envelope
        }
        else {
            /* Voice is in the on state. */
            if (v->sema == VOICE_MAX_SEMA) {
                /* Cannot support more simultaneous on states, so drop
                   the voice. */
                VOICE_ALLOC_LOG("Dropping note_on %d\n", note);
            }
            else {
                v->sema++;
                // FIXME: trigger the envelope
            }
        }
    }
}

void voice_alloc_note_off(struct voice_alloc *va, uint8_t note) {
    ASSERT(note < NB_NOTES);
    uint8_t voice_nb = va->note_to_voice[note];
    if (voice_nb == VOICE_NONE) {
        VOICE_ALLOC_LOG("Dropping unknown note_off %d\n", note);
    }
    else {
        struct voice_meta *v = &va->voice[voice_nb];
        ASSERT(note == v->note);
        if (0 == v->sema) {
            VOICE_ALLOC_LOG("Dropping spurious note_off %d\n", note);
        }
        else {
            v->sema--;
            if (0 == v->sema) {
                /* Move from on to off queue. */
                voice_alloc_remove(va, voice_nb);
                voice_alloc_insert_before(va, VOICE_OFF, voice_nb);
                // FIXME: send note off to envelope
            }
            else {
                /* Nothing to do.  Note will stay in the on state
                   until we have compensated all on events with off
                   events. */
            }
        }
    }
}


static inline void voice_alloc_init(struct voice_alloc *va) {
    /* This will initialize all sema=0 meaning voices are off.  The
       note value is 0 which is ok as a dummy value.  The prev,next
       pointers are updated in the following step. */
    memset(va,0,sizeof(*va));

    /* Empty note to voice map. */
    memset(va->note_to_voice, VOICE_NONE, sizeof(va->note_to_voice));


    /* Initialize both lists to contain only the head element,
       meaning they are empty. */
    va->voice[VOICE_ON].prev = VOICE_ON;
    va->voice[VOICE_ON].next = VOICE_ON;
    va->voice[VOICE_OFF].prev = VOICE_OFF;
    va->voice[VOICE_OFF].next = VOICE_OFF;

    /* Add all the notes to the off list. */
    for(uint8_t n = 0; n < NB_VOICES; n++) {
        struct voice_meta *v = &va->voice[n];
        v->prev = VOICE_NONE;
        v->next = VOICE_NONE;
        v->sema = 0; // off
        v->note = NOTE_NONE;
        voice_alloc_insert_before(va, VOICE_OFF, n);
    }
}

static inline void voice_alloc_dump_chain(struct voice_alloc *va, uint8_t head_nb) {
    for (uint8_t n = va->voice[head_nb].next;
         n != head_nb;
         n = va->voice[n].next) {
        if (head_nb == VOICE_OFF) {
            VOICE_ALLOC_LOG(" %d", n);
        }
        else {
            struct voice_meta *v = &va->voice[n];
            VOICE_ALLOC_LOG(" %d[%d]", n, v->sema);
        }
    }
}

static inline void voice_alloc_dump(struct voice_alloc *va) {
    voice_alloc_dump_chain(va, VOICE_OFF);
    VOICE_ALLOC_LOG(" |");
    voice_alloc_dump_chain(va, VOICE_ON);
    VOICE_ALLOC_LOG("\n");
}

static inline int voice_alloc_voice_state(struct voice_alloc *va, uint8_t voice_nb) {
    /* The sema will tell us whether the voice is on or off.  Note
       that also tells us which queue it is in. */
    return !!(va->voice[voice_nb].sema);
}


#endif
