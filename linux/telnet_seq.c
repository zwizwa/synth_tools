// Playground for midi + audio synth.
// Midi part is cloned from jack_midi.c


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "macros.h"
#include "sysex.h"


#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#include "assert_read.h"

#include "jack_tools.h"

#define LOG(...) fprintf(stderr, __VA_ARGS__)

#include "mod_sequencer.c"
struct app {
    struct sequencer sequencer;
    void *midi_out_buf;
};
struct app app;

// Send midi data out over a jack port.
static inline void send_midi(void *out_buf, jack_nframes_t time,
                             const void *data_buf, size_t nb_bytes) {
    //LOG("%d %d %d\n", frames, time, (int)nb_bytes);
    void *buf = jack_midi_event_reserve(out_buf, time, nb_bytes);
    if (buf) memcpy(buf, data_buf, nb_bytes);
}
static inline void send_cc(void *out_buf, int chan, int cc, int val) {
    const uint8_t midi[] = {0xB0 + (chan & 0x0F), cc & 0x7F, val & 0x7F};
    send_midi(out_buf, 0, midi, sizeof(midi));
}
static inline void send_control_byte(void *out_buf, uint8_t byte) {
    send_midi(out_buf, 0, &byte, 1);
}
static inline void send_start(void *out_buf) { send_control_byte(out_buf, 0xFA); }
static inline void send_stop(void *out_buf)  { send_control_byte(out_buf, 0xFC); }
void app_sequencer_tick(struct sequencer *seq, const union pattern_event *ev) {
    struct app *app = (void*)seq;
    const uint8_t *msg = ev->u8;
    LOG("tick %02x %02x %02x %02x\n", msg[0], msg[1], msg[2], msg[3]);

    if (msg[0] < 16) {
        // FIXME: msg[0] is midi port, make numerical mapping
        send_midi(app->midi_out_buf, 0, msg + 1, 3);
    }
    else {
        LOG("unsupported event tag %d\n", msg[0]);
    }
}


/* TOOLS */
#define NB_EL(x) (sizeof(x)/sizeof((x)[0]))
#define FOR_IN(i,a) for(i=0; i<NB_EL(a); i++)

/* Map midi note to octave, note */
#define FREQ_TO_INC(freq)  (((freq) / SYNTH_SAMPLE_RATE) * PHASOR_PERIOD)




/* JACK */

#define FOR_MIDI_IN(m) \
    m(midi_in)         \

#define FOR_MIDI_OUT(m) \
    m(midi_out)

FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

static int count = 0;

static inline void process_midi(jack_nframes_t nframes) {
    void *midi_in_buf  = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {
        // LOG("\rmidi %d ", count++);
        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        // const uint8_t *msg = event.buffer;
        // LOG_HEX("seq:", msg, event.size);
        LOG(" seq: %d\r", count++);
        // ...
    }
}
static int process (jack_nframes_t nframes, void *arg) {
    /* Order is important. */
    process_midi(nframes);
    return 0;
}

#define TELNET_WORD_MODE
#include "mod_telnet.c"

void telnet_write_output(struct telnet *, const uint8_t *bytes, uintptr_t len) {
    fwrite(bytes, 1, len, stdout);
    fflush(stdout);
}

/* Map escape codes to command. */
struct escapes {
    const char *esc;
    void (*op)(struct telnet *);
};
void f1(struct telnet *t) { LOG("f1\n"); }
void f2(struct telnet *t) { LOG("f2\n"); }
void f3(struct telnet *t) { LOG("f3\n"); }
void f4(struct telnet *t) { LOG("f4\n"); }

const struct escapes escapes[] = {
    {"[11~",f1},
    {"[12~",f2},
    {"[13~",f3},
    {"[14~",f4},
    {},
};

// handler just prints a representation of the event
void telnet_event(struct telnet *t, uintptr_t event) {
    uint8_t byte = event & 0xFF;
    event &= ~0xff;

    switch(event) {
    case TELNET_EVENT_INTERRUPT:
        LOG("<INTERRUPT>\n");
        break;
    case TELNET_EVENT_CONTROL:
        LOG("<CONTROL:%d>\n", byte);
        if (byte == 4) { /* CTRL-D */ LOG("exiting\n"); exit(0); }
        else if (byte == 12) { telnet_clear(t); }
        break;
    case TELNET_EVENT_ESCAPE: {
        char e0[t->nb_esc+1];
        memcpy(e0, t->esc, t->nb_esc);
        e0[t->nb_esc] = 0;
        for (const struct escapes *e = &escapes[0]; e->esc; e++) {
            if (!strcmp(e0, e->esc)) {
                LOG("ESC %s %p\n", e0, e->op);
                e->op(t);
                return;
            }
        }
        LOG("<ESC:");
        for(uint32_t i=0; i<t->nb_esc; i++) {
            LOG("%c", t->esc[i]);
        }
        LOG(">\n");
        break;
    }
    case TELNET_EVENT_LINE:
        LOG("<LINE:");
        for(uint32_t i=0; i<t->nb_char; i++) {
            LOG("%c", t->line[i]);
        }
        LOG(">\n");
        break;
    case TELNET_EVENT_FLUSH:
        break;
    case TELNET_EVENT_PROMPT:
        // TELNET_WRITE_OUTPUT(t, "> ");
        TELNET_WRITE_OUTPUT(t, ":");
        break;
    }
}


int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "telnet_seq"; // argv[1];

    sequencer_init(&app.sequencer, app_sequencer_tick);
    sequencer_restart(&app.sequencer);

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
    FOR_MIDI_OUT(REGISTER_JACK_MIDI_OUT);

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* Run telnet / tty server on stdout. */
    struct telnet s;
    telnet_init(&s, telnet_write_output, telnet_event);
    for(;;) {
        telnet_tick(&s, getchar());
    }
    return 0;
}

