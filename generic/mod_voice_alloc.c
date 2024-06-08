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

#ifndef NB_VOICES
#define NB_VOICES 64
#endif

#ifndef NB_NOTES
#define NB_NOTES 128
#endif

#ifndef VOICE_ALLOC_LOG
#define VOICE_ALLOC_LOG LOG
#endif

struct voice_meta {
    uint8_t prev;  // previous element in on or off queue
    uint8_t next;  // next ...
    uint8_t sema;  // 0=off, >0 nb_on
    uint8_t note;  // voice->note map
};

struct voice_alloc {
    struct voice_meta voice[NB_VOICES];
    uint8_t note_to_voice[NB_NOTES]; 
    uint8_t next_on;
    uint8_t next_off;
};

static inline void voice_alloc_init(struct voice_alloc *va) {
    /* This will initialize all sema=0 meaning voices are off.  The
       note value is 0 which is ok as a dummy value.  The prev,next
       pointers are updated in the following step. */
    memset(va,0,sizeof(*va));

    /* Empty note to voice map. */
    memset(va->note_to_voice, VOICE_NONE, sizeof(va->note_to_voice));

    /* All voices are in the off queue, and the on queue is empty. */
    va->next_on = VOICE_NONE;
    va->next_off = 0;

    /* Initialize next/prev for first, mid and last elements. */
    struct voice_meta *v = &va->voice[0];
    v[0].prev = VOICE_NONE;
    v[0].next = 1;
    for(uint8_t n = 1; n < NB_VOICES-1; n++) {
        v[n].prev = n-1;
        v[n].next = n+1;
    }
    v[NB_VOICES-1].prev = NB_VOICES-2;
    v[NB_VOICES-1].next = VOICE_NONE;

}

static inline void voice_alloc_log_chain(struct voice_alloc *va, const char *tag, uint8_t n) {
    VOICE_ALLOC_LOG("%s: (", tag);
    for (; n != VOICE_NONE; n = va->voice[n].next) {
        VOICE_ALLOC_LOG(" %d", n);
    }
    VOICE_ALLOC_LOG(" )\n");
}

static inline void voice_alloc_dump(struct voice_alloc *va) {
    voice_alloc_log_chain(va, "on", va->next_on);
    voice_alloc_log_chain(va, "off", va->next_off);
}

#endif
