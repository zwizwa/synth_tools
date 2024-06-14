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
#include "alsa_tools.h"

// The synth engine is part of the rkt/cgen.rkt test suite.
// See rkt/test-cgen.rkt
#include "cgen_synth_out.h"


/* VOICE ALLOCATOR */
#define NB_VOICES 64
#include "mod_voice_alloc.c"


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


struct synth {

    /* Per voice state data. */
    struct voice voice[NB_VOICES];

    /* Synth DSP state */
    struct synth_state state;

    /* Voice allocator state */
    struct voice_alloc voice_alloc;

    /* JACK */
    jack_ringbuffer_t *ringbuffer;

    /* ALSA */
    snd_seq_t *seq;
    snd_midi_event_t *alsa_decoder;
    int alsa_client_id;
    int alsa_port_id;

};

struct synth_command {
    uint8_t msg[4];
};

/* Cross-link */
#include "uct_offsetof.h"

#define DEF_TO_SYNTH(substruct)                 \
    DEF_FIELD_TO_PARENT(                        \
        substruct##_to_synth,                   \
        struct synth,                           \
        struct substruct,                       \
        substruct)                              \

DEF_TO_SYNTH(voice_alloc)



/* MIDI */
void synth_note_on (struct synth *, uint8_t note, uint8_t velocity);
void synth_note_off(struct synth *, uint8_t note, uint8_t velocity);

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

void voice_event(struct voice_alloc *va,
                 uint8_t voice_nb,
                 uint8_t event,
                 uint8_t note,
                 uint8_t velocity) {
    LOG("voice_event(%d,%x,%d,%d)\n", voice_nb, event, note, velocity);
    struct synth *synth = voice_alloc_to_synth(va);
    switch(event) {
    case VOICE_EVENT_ON:
        synth->voice[voice_nb].note_inc = note_to_inc(note);
        break;
    case VOICE_EVENT_OFF:
        /* FIXME: I wonder if this causes notes to get stuck in a high
           phase, adding DC */
        synth->voice[voice_nb].note_inc = 0;
        break;
    }
}


void synth_note_on(struct synth *x, uint8_t note, uint8_t velocity) {
    voice_alloc_note_on(&x->voice_alloc, note, velocity);
}

void synth_note_off(struct synth *x, uint8_t note, uint8_t velocity) {
    voice_alloc_note_off(&x->voice_alloc, note, velocity);
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
    voice_alloc_init(&x->voice_alloc, voice_event);
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

static void handle_midi(const uint8_t *msg, int size) {
    // LOG("\rmidi %d ", count++);
    // LOG_HEX("synth:", msg, event.size);
    if (size == 3 &&
        msg[0] == 0xB0 && // CC channel 0
        (msg[1] >= 23) && // CC num on Easycontrol 9
        (msg[1] <= 31)) {
        // ...
    }
    else if (size == 3 &&
             msg[0] == 0x90) { // Note on channel 0
        uint8_t note = msg[1];
        uint8_t vel  = msg[2];
        if (vel == 0) {
            synth_note_off(&synth, note, vel);
        }
        else {
            synth_note_on(&synth, note, vel);
        }
    }
    else if (size == 3 &&
             msg[0] == 0x80) { // Note off channel 0
        uint8_t note = msg[1];
        uint8_t vel  = msg[2];
        synth_note_off(&synth, note, vel);
    }
}

static inline void process_midi(jack_nframes_t nframes) {

    /* Jack midi */
    void *midi_in_buf  = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {
        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        handle_midi(event.buffer, event.size);
    }

    /* Alsa midi */
    struct synth_command c;
    int nr;
    while (sizeof(c) == (nr = jack_ringbuffer_read(
               synth.ringbuffer, (void*)&c, sizeof(c)))) {
        // FIXME: Make a better protocol instead of hardcoded size.
        const uint8_t *m = &c.msg[1];
        // LOG("synth: midi from alsa %02x %02x %02x\n", m[0], m[1], m[2]);
        handle_midi(m, 3);
    }
    ASSERT(nr == 0);

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


#define MAX_MIDI 16


static void to_synth(struct synth *synth, struct synth_command *c) {
    int n;
    while (sizeof(*c) != (n = jack_ringbuffer_write(synth->ringbuffer, (void*)c, sizeof(*c)))) {
        /* If ringbuffer is full we pause the MIDI thread and retry.
           There is no other synchronization mechanism. */
        ASSERT(n == 0);
        struct timespec nanoseconds = {
            .tv_sec = 0,
            .tv_nsec = 1000000,
        };
        ASSERT_ERRNO(nanosleep(&nanoseconds, NULL));
    }
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

    synth.ringbuffer = jack_ringbuffer_create(16 * sizeof(struct synth_command));


    /* Support ALSA MIDI */
    ALSA_ASSERT(snd_seq_open(&synth.seq, "hw", SND_SEQ_OPEN_INPUT, 0));
    snd_seq_set_client_name(synth.seq, "synth");
    synth.alsa_client_id = snd_seq_client_id(synth.seq);
    synth.alsa_port_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            synth.seq, "synth_midi",
            SND_SEQ_PORT_CAP_WRITE |
            SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_HARDWARE));
    ALSA_ASSERT(snd_midi_event_new(MAX_MIDI, &synth.alsa_decoder));
    snd_midi_event_reset_decode(synth.alsa_decoder);
    snd_midi_event_no_status(synth.alsa_decoder, 1);


    /* Input loop. */
    int npfd = snd_seq_poll_descriptors_count(synth.seq, POLLIN);
    struct pollfd pfd[npfd + 1]; // One extra for stdin
    snd_seq_poll_descriptors(synth.seq, pfd + 1, npfd, POLLIN);

    pfd[0].fd = 0;
    pfd[0].events = POLLIN;


    for(;;) {
        if (poll(pfd, 1 + npfd, -1 /*inf*/) > 0) {

            /* 3 cases to handle: one of te pfds has an error, input
               ready, sequencer ready */

            for (int i=0; i<npfd+1; i++) {
                if (pfd[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    exit(1);
                }
            }
            if (pfd[0].revents & POLLIN) {
                // FIXME: only used to signal exit
                uint8_t buf[4];
                assert_read(0, buf, sizeof(buf));
                exit(1);
            }
            else {
                do {
                    snd_seq_event_t *ev;
                    snd_seq_event_input(synth.seq, &ev);
                    // LOG("synth: event\n");
                    switch(ev->type) {
                    case SND_SEQ_EVENT_CLOCK:
                        // Not needed until we have LFO sync.
                        // LOG("event: clock\n");
                        break;
                    case SND_SEQ_EVENT_PORT_SUBSCRIBED:
                        LOG("event: port subscribed\n");
                        break;
                    default: {
                        /* Not handling all snd_seq_event_type separately.
                           Try to convert it to midi. */
                        static unsigned char buf[MAX_MIDI];

                        long count = snd_midi_event_decode(
                            synth.alsa_decoder, buf, sizeof(buf), ev);
                        if (count > 0) {
                            if (1) {
                                LOG("synth: ALSA MIDI %d:%d",
                                    ev->source.client, ev->source.port);
                                for (long i=0; i<count; i++) { LOG(" %02x", buf[i]); }
                                LOG("\n");
                            }
                            switch(buf[0]) {
                            case 0x80:
                            case 0x90:
                            case 0xb0: {
                                struct synth_command c = { .msg = {0, buf[0], buf[1], buf[2]}};
                                to_synth(&synth, &c);
                                break;
                            }
                            }
                        }
                        else {
                            /* Event not supported or decoder error. */
                            LOG("WARNING: decode=%d, event=%d\n", count, ev->type);
                        }
                        break;
                    }}
                    snd_seq_free_event(ev);

                }
                while (snd_seq_event_input_pending(synth.seq, 0) > 0);
            }
        }
    }

    return 0;
}

