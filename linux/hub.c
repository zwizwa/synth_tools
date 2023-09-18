/* Erlang MIDI hub.

   Handles all midi/Erlang routing.
   Hosts sequencer / arpeggiator.

   Clock is always slave mode here to keep things flexible.
   In my setup, clock.c is master clock.

   Note that this has all equipment hardcoded.  I currently do not see
   the point in adding a layer of configuration abstraction.  Easy
   enough to recompile in the current setup, so all config is in C, or
   C generated from compile-time config.  Later it might become
   obvious how to separate this out into config and generic code.

*/

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"

#include "jack_tools.h"


/* JACK */
#define FOR_MIDI_IN(m) \
    m(clock_in)     \
    m(fire_in)      \
    m(easycontrol)  \

#define FOR_MIDI_OUT(m) \
    m(tb03)         \
    m(fire_out)     \
    m(volca_keys)   \
    m(volca_bass)   \
    m(volca_beats)  \
    m(synth)        \
    m(pd_out)       \


FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

static inline void *midi_out_buf(jack_port_t *port, jack_nframes_t nframes) {
    void *buf = jack_port_get_buffer(port, nframes);
    jack_midi_clear_buffer(buf);
    return buf;
}
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



static uint32_t phase = 0;
static uint32_t running = 0;



static inline void process_clock_in(jack_nframes_t nframes) {
    void *pd_out_buf = midi_out_buf(pd_out, nframes);

    FOR_MIDI_EVENTS(iter, clock_in, nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 1) {
            switch(msg[0]) {
            case 0xFA: // start
                running = 1;
                phase = 0;
                break;
            case 0xFB: // continue
                running = 1;
                break;
            case 0xFC: // stop
                running = 0;
                break;
            case 0xF8: { // clock
                if (running) {
                    uint32_t qn = phase == 0; // one quarter note
                    uint32_t en = (phase % 12) == 0; // one eight note

                    /* For Pd Note On/Off is not used as it requires begin/end of events.
                       Use CC to map better to single-ended events. */
                    if (qn) {
                        send_cc(pd_out_buf, 0, 0, 0);
                    }
                    if (en) {
                        send_cc(pd_out_buf, 0, 0, 1);
                    }
                }
                phase = (phase + 1) % 24;
                break;
            }
            }
        }
    }
}

static inline void process_easycontrol_in(jack_nframes_t nframes) {
    FOR_MIDI_EVENTS(iter, easycontrol, nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 3) {
            if ((msg[0] & 0xF0) == 0xB0) { // CC
                uint8_t cc  = msg[1];
                uint8_t val = msg[2];
                LOG("cc=%d, val=%d\n", cc, val);
                // I want this to go to Erlang so I can log it in emacs.
            }
        }
    }
}


static int process (jack_nframes_t nframes, void *arg) {
    /* Order is important. */
    process_clock_in(nframes);
    process_easycontrol_in(nframes);

    return 0;
}


int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "hub"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);
    FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
    FOR_MIDI_OUT(REGISTER_JACK_MIDI_OUT);

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* The main thread blocks on stdin.  The protocol is lowest common
       denominator.  While these are written to interface to Erlang,
       let's use midi as the main protocol so it is easier to reuse in
       different configurations. */
    for(;;) {
        // FIXME: Current function is just to exit when stdin is closed.
        uint8_t buf[1];
        assert_read(0, buf, sizeof(buf));
        exit(1);
    }
    return 0;
}

