// Playground for midi + audio synth.
// Midi part is cloned from jack_midi.c


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

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


/* SYNTH */

#define SYNTH_NB_VARS 3

typedef uint32_t phasor_t;

struct voice {
    phasor_t note_inc;  /* 0 == off */
    phasor_t note_state;
};
struct synth {
    int note2voice[128];
    struct voice voice[64];
};

void synth_note_on(struct synth *, int note);
void synth_note_off(struct synth *, int note);
void synth_init(struct synth *);
void synth_run(struct synth *, float *vec, int n);

#include <stdio.h>
#include <math.h>
#include <string.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)

/* CONFIG */
#ifndef SYNTH_SAMPLE_RATE
#define SYNTH_SAMPLE_RATE 48000.0
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

const uint8_t midi_tab[128] = {
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
    LOG("%d -> (%d,%d,%d,%d)\n", note, octave, n, note_tab[n], p);
    return p;
}


#else

#define NOTE_TO_FREQ(note) (REF_FREQ * POW2((((note) - REF_NOTE) / NOTES_PER_OCTAVE)))
#define NOTE_TO_INC(note)  (FREQ_TO_INC(NOTE_TO_FREQ(note)))
#define POW2(x) pow(2,x)

phasor_t note_to_inc(int i_note) {
    double note = i_note;
    /* 60 -> 440Hz */
    double freq = NOTE_TO_FREQ(note);
    double inc = FREQ_TO_INC(freq);
    phasor_t i_inc = (inc + 0.5);
    LOG("freq = %f -> %f, %d\n", note, freq, i_inc);
    return i_inc;
}
#endif

int voice_alloc(struct synth *x) {
    unsigned int v;
    FOR_IN(v, x->voice) {
        if (x->voice[v].note_inc == 0) return v;
    }
    /* This is not good, but better than doing nothing.  FIXME: Use
       current envelope value to perform selection.  Data org: keep
       envolopes together. */
    return 0;
}

void synth_note_on(struct synth *x, int note) {
    int v = voice_alloc(x);
    x->note2voice[note % 128] = v;
    x->voice[v].note_inc = note_to_inc(note % 128);
}
void synth_note_off(struct synth *x, int note) {
    int v = x->note2voice[note % 128];
    x->note2voice[note % 128] = 0;
    x->voice[v].note_inc = 0;
}

/* MIDI note state */
// FIXME: no float!
float sum_tick_saw(struct synth *x) {
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
void synth_run(struct synth *x, float *vec, int n) {
    // FIXME: update parameters
    int i;
    for (i=0; i<n; i++) {
        vec[i] = sum_tick_saw(x);
    }
}

void synth_init(struct synth *x) {
    bzero(x, sizeof(*x));
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

static int count = 0;

static inline void process_midi(jack_nframes_t nframes) {
    void *midi_in_buf  = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {
        LOG("\rmidi %d ", count++);
        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        const uint8_t *msg = event.buffer;
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

    jack_default_audio_sample_t *dst =
        jack_port_get_buffer(audio_out, nframes);
#if 1
    synth_run(&synth, dst, nframes);
#else
    for (int t=0; t<nframes; t++) {
        dst[t] = sig;
        sig += 0.01;  // quick and dirty buzz
    }
#endif
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

