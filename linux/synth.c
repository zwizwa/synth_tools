// Playground for midi + audio synth.
// Midi part is cloned from jack_midi.c


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "macros.h"
#include "sysex.h"


#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#include "assert_read.h"

#include "jack_tools.h"

// The synth engine is part of the rkt/cgen.rkt test suite.
// See rkt/test-cgen.rkt
#include "cgen_synth_out.h"

/* SYNTH */

#define SYNTH_NB_VARS 3

typedef uint32_t phasor_t;


#if 0
// Don't use this.  It's not composable and I do want that.
#include "cproc.h"
#define for_osc_state(m) m(phasor_t,note_state) m(phasor_t,note_inc)
#define for_osc_input(m) 
#define for_osc_config(m)
#define for_osc_param(m)
DEF_PROC(osc, s, c, p, i) {
    s->note_state += s->note_inc;
}
#endif


struct voice {
    phasor_t note_inc;  /* 0 == off */
    phasor_t note_state;
};
#define NB_VOICES 64
#define NO_VOICE 255

#define NB_NOTES 128
#define MAX_NOTE_NB_ON 255
struct synth {
    /* Map MIDI note number to the voice that is currently allocated
       to it.  Can be NO_VOICE or 0 .. NB_VOICES-1 */
    uint8_t note2voice[NB_NOTES];

    /* Semaphore keeping track of how many note on events we've seen
       not cancelled by a note off event. */
    uint8_t note_nb_on[NB_NOTES];

    /* Queues for voice allocation.  Notes that are in the off state
       (but maybe still ringing their release state) are prioritized
       over those in the on state. */
    struct cbuf q_note_on;  uint8_t q_note_on_buf[NB_VOICES];
    struct cbuf q_note_off; uint8_t q_note_off_buf[NB_VOICES];

    /* Per voice state data. */
    struct voice voice[NB_VOICES];

    /* Synth DSP state */
    struct synth_state state;

};

/* MIDI */
void synth_note_on (struct synth *, uint8_t note);
void synth_note_off(struct synth *, uint8_t note);

void synth_init(struct synth *);
void synth_run(struct synth *, float *vec, int n);

#include <stdio.h>
#include <math.h>
#include <string.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)

/* CONFIG */
#ifndef SYNTH_SAMPLE_RATE
#define SYNTH_SAMPLE_RATE 44100.0
#endif

/* Implementation constants. */
#define PHASOR_PERIOD 4294967296.0 // 32 bit phasor
#define NOTES_PER_OCTAVE 12.0
#define REF_FREQ 440.0
#define REF_NOTE 69.0

/* TOOLS */
#define NB_EL(x) (sizeof(x)/sizeof((x)[0]))
#define FOR_IN(i,a) for(i=0; i<NB_EL(a); i++)


/* Map midi note to octave, note */
#define FREQ_TO_INC(freq)  (((freq) / SYNTH_SAMPLE_RATE) * PHASOR_PERIOD)

#if 1
/* Table based. */

/* Create the phasor increments for an equally tempered chromatic
   scale using a floating point sequence.  Other octaves are derived
   from these by shifting. */

#define SEMI          0.9438743126816935 // 2 ^ {1/12}
#define MIDI_NOTE_127 12543.853951415975 // 440 ^ {2^{127-69/12}}, frequency of MIDI note 127

#define N0 (SEMI*N1)
#define N1 (SEMI*N2)
#define N2 (SEMI*N3)
#define N3 (SEMI*N4)
#define N4 (SEMI*N5)
#define N5 (SEMI*N6)
#define N6 (SEMI*N7)
#define N7 (SEMI*N8)
#define N8 (SEMI*N9)
#define N9 (SEMI*N10)
#define N10 (SEMI*N11)
#define N11 FREQ_TO_INC(MIDI_NOTE_127)

static const phasor_t note_tab[12] = {
    N0, N1, N2,  N3,  // 116 - 119
    N4, N5, N6,  N7,  // 120 - 123
    N8, N9, N10, N11, // 124 - 127
};

/* Create a midi note -> octave, note map */
#define NOTE(o,n) \
    ((((o) & 15) << 4) | ((n) & 15))
#define OCTAVE(o) \
    NOTE(o,0), NOTE(o,1), NOTE(o,2),  NOTE(o,3), \
    NOTE(o,4), NOTE(o,5), NOTE(o,6),  NOTE(o,7), \
    NOTE(o,8), NOTE(o,9), NOTE(o,10), NOTE(o,11)

const uint8_t midi_tab[NB_NOTES] = {
    NOTE(10,4), NOTE(10,5), NOTE(10,6),  NOTE(10,7),
    NOTE(10,8), NOTE(10,9), NOTE(10,10), NOTE(10,11),
    OCTAVE(9),
    OCTAVE(8), OCTAVE(7), OCTAVE(6),
    OCTAVE(5), OCTAVE(4), OCTAVE(3),
    OCTAVE(2), OCTAVE(1), OCTAVE(0),
};

/* Combine both tables. */
phasor_t note_to_inc(int note) {
    int octave_note = midi_tab[note & 127];
    int octave = octave_note >> 4;
    int n = octave_note & 15;
    phasor_t p = note_tab[n] >> octave;
    // LOG("note to inc %d -> (%d,%d,%d,%d)\n", note, octave, n, note_tab[n], p);
    return p;
}


#else

#define NOTE_TO_FREQ(note) (REF_FREQ * POW2((((note) - REF_NOTE) / NOTES_PER_OCTAVE)))
#define NOTE_TO_INC(note)  (FREQ_TO_INC(NOTE_TO_FREQ(note)))
#define POW2(x) pow(2,x)

phasor_t note_to_inc(uint8_t b_note) {
    double note = b_note;
    /* 60 -> 440Hz */
    double freq = NOTE_TO_FREQ(note);
    double inc = FREQ_TO_INC(freq);
    phasor_t i_inc = (inc + 0.5);
    LOG("freq = %f -> %f, %d\n", note, freq, i_inc);
    return i_inc;
}
#endif


// INVARIANT: Voices are either in the on or the off queue.
uint8_t voice_alloc(struct synth *x) {
    uint8_t v = 0;
    /* Get last voice that was turned off. */
    if (1 == cbuf_read(&x->q_note_off, &v, 1)) goto gotit;
    /* If there is none, get last voice that was turned on. */
    if (1 == cbuf_read(&x->q_note_on, &v, 1)) goto gotit;
    ERROR("internal error: voice_alloc failed\n");
  gotit:
    cbuf_write(&x->q_note_on, &v, 1);
    return v;
}

void synth_note_on(struct synth *x, uint8_t note) {
    ASSERT(note < NB_NOTES);
    if (x->note_nb_on[note] < MAX_NOTE_NB_ON) {
        /* MIDI is a bit problematic when it comes to modeling
           multiple instances of the same note.  We solve that by
           keeping track of the difference between note on and note
           off events (a semaphore), such that the last note off turns
           off the voice.  We expect this to happen for a "couple of
           notes" up to MAX_NOTE_NB_ON */
        x->note_nb_on[note]++;
    }
    else {
        /* This degenerates when there are more than 255 simultaneous notes.
           Then further note on events get ignored.  When this happens the
           voice will turn off after 255 note off events. */
        LOG("WARNING: ignoring note_on\n");
        return;
    }
    uint8_t v;
    if (x->note_nb_on[note] == 1) {
        /* The note was previously off, so allocate a new voice. */
        v = voice_alloc(x);
        x->note2voice[note] = v;
        x->voice[v].note_inc = note_to_inc(note);
    }
    else {
        /* The note was previously on and phasor was set in a previous
           note on message, so update only the envelope. */
        v = x->note2voice[note];
    }
    /* Each (subsequent) note on event retriggers the envelope. */
    // FIXME: trigger_voice_envelope_note_on(v)
}

void synth_note_off(struct synth *x, uint8_t note) {
    ASSERT(note < NB_NOTES);
    if (x->note_nb_on[note] == 0) {
        /* We did not register a previous on event.  Ignore. */
        LOG("WARNING: ignoring note_off\n");
        return;
    }
    x->note_nb_on[note]--;
    if (x->note_nb_on[note] > 0) {
        /* We are waiting for more note off events.  Ignore. */
        return;
    }
    // FIXME: trigger_voice_envelope_note_off(v)

    uint8_t v = x->note2voice[note];

    /* Move the voice into the note off queue, so it will be stolen
       before any on notes. */
    cbuf_write(&x->q_note_off, &v, 1);

    /* Remove the voice from the note on queue. */
    // FIXME

    /* Remove dangling references */
    x->note2voice[note] = NO_VOICE;
}


/* MIDI note state */
// FIXME: no float!
// FIXME: replace voice with cproc
float sum_tick_osc(struct synth *x) {
    unsigned int v;
    int sum = 0;
    FOR_IN(v, x->voice) {
        if (x->voice[v].note_inc) {
            /* Shift is arbitrary, but we interpret phasor as signed. */
            int p = x->voice[v].note_state;
            sum += (p >> 4);
            x->voice[v].note_state += x->voice[v].note_inc;
        }
    }
    return (1.0 / PHASOR_PERIOD) * ((float)sum);
}
#if 0
float sum_tick_square(struct synth *x) {
    unsigned int v;
    unsigned int accu = 0;
    FOR_IN(v, x->voice) {
        if (x->voice[v].note_inc) {
            /* Shift is arbitrary, but we interpret phasor as signed. */
            unsigned int bit = x->voice[v].note_state & 0x80000000;
            // accu ^= bit;
            accu |= bit;
            x->voice[v].note_state += x->voice[v].note_inc;
        }
    }
    return (1.0 / PHASOR_PERIOD) * ((float)accu);
}
#endif

void synth_run(struct synth *x, float *vec, int n) {
    // FIXME: update parameters
#if 0
    for (int i=0; i<n; i++) {
        vec[i] = sum_tick_osc(x);
    }
#else
    struct synth_in in = {};
    unsigned int v;
    FOR_IN(v, x->voice) { in.i0[v] = ((T)(x->voice[v].note_inc)) / 0xFFFFFFFF; };
    struct synth_out out;
    synth_update(&x->state, &in, &out);
    for(int i=0; i<n; i++) {
        vec[i] = 0.1 * out.o0[i];
    }
#endif
}

void synth_init(struct synth *x) {
    bzero(x, sizeof(*x));
    CBUF_INIT(x->q_note_on);
    CBUF_INIT(x->q_note_off);
    // Initially, all voices are in the off queue.
    for (uint8_t v=0; v<NB_VOICES; v++) {
        cbuf_write(&x->q_note_off, &v, 1);
    }
}

struct synth synth;



/* JACK */

#define FOR_MIDI_IN(m) \
    m(midi_in)         \

#define FOR_AUDIO_OUT(m) \
    m(audio_out)

FOR_MIDI_IN(DEF_JACK_PORT)
FOR_AUDIO_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

//static int count = 0;

static inline void process_midi(jack_nframes_t nframes) {
    void *midi_in_buf  = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {
        // LOG("\rmidi %d ", count++);
        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        const uint8_t *msg = event.buffer;
        // LOG_HEX("synth:", msg, event.size);
        if (event.size == 3 &&
            msg[0] == 0xB0 && // CC channel 0
            (msg[1] >= 23) && // CC num on Easycontrol 9
            (msg[1] <= 31)) {
            // ...
        }
        else if (event.size == 3 &&
                 msg[0] == 0x90) { // Note on channel 0
            uint8_t note = msg[1];
            uint8_t vel  = msg[2];
            if (vel == 0) {
                synth_note_off(&synth, note);
            }
            else {
                synth_note_on(&synth, note);
            }
        }
        else if (event.size == 3 &&
                 msg[0] == 0x80) { // Note off channel 0
            uint8_t note = msg[1];
            // uint8_t vel  = msg[2];
            synth_note_off(&synth, note);
        }
    }
}
static inline void process_audio(jack_nframes_t nframes) {
    //LOG("\raudio %d ", count++);
    float sig = 0;
    (void)sig;

    jack_nframes_t block_size = 64;  // FIXME: Hardcoded in the rkt file
    jack_default_audio_sample_t *dst = jack_port_get_buffer(audio_out, nframes);

    while (nframes > 0) { // FIXME: assuming it is a multiple of block_size
        synth_run(&synth, dst, nframes);
        nframes -= block_size;
        dst += block_size;
    }
}
static int process (jack_nframes_t nframes, void *arg) {
    /* Order is important. */
    process_midi(nframes);
    process_audio(nframes);
    return 0;
}

int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "synth"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
    FOR_AUDIO_OUT(REGISTER_JACK_AUDIO_OUT);

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    synth_init(&synth);

    /* Input loop. */
    for(;;) {
        // FIXME: only used to signal exit
        uint8_t buf[4];
        assert_read(0, buf, sizeof(buf));
        exit(1);
    }

    return 0;
}

