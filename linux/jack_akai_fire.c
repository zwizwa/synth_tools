// MIDI Clock code lifted from studio/c_src/jack_midi.c

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"

#include <jack/jack.h>
#include <jack/midiport.h>

/* AKAI FIRE */


/* JACK */
static jack_port_t *midi_out = NULL;
static jack_port_t *midi_in = NULL;
static jack_client_t *client = NULL;

// Send midi data out over a jack port.
static inline void send_midi(void *out_buf, jack_nframes_t time,
                             const void *data_buf, size_t nb_bytes) {
    //LOG("%d %d %d\n", frames, time, (int)nb_bytes);
    void *buf = jack_midi_event_reserve(out_buf, time, nb_bytes);
    if (buf) memcpy(buf, data_buf, nb_bytes);
}

#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

#define ROWS 4
#define COLS 16
uint8_t pads[ROWS][COLS] = {};

void pad(uint8_t row, uint8_t col) {
    pads[row][col] ^= 1;
    LOG("%d %d = %d\n", row, col, pads[row][col]);
}

static int process (jack_nframes_t nframes, void *arg) {

    void *midi_in_buf  = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {

        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        const uint8_t *msg = event.buffer;

        if (event.size == 3) {
            if (msg[0] == 0xB0) { // CC channel 0
                // sprintf(buf, "cc %d %d;\n", msg[1], msg[2]);
            }
            else if (msg[0] == 0x90) { // Note on channel 0
                // sprintf(buf, "on %d %d;\n", msg[1], msg[2]);
                uint8_t note = msg[1];
                if (note >= 54) {
                    uint8_t col = note - 54;
                    uint8_t row = col / 16;
                    col -= row * 16;
                    pad(row, col);
                }
            }
            else if (msg[0] == 0x80) { // Note off channel 0
                // sprintf(buf, "off %d %d;\n", msg[1], msg[2]);
            }
        }
        else {
        }
    }

    return 0;
}


int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "jack_akai_fire"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    ASSERT(midi_in = jack_port_register(
               client, "in",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));

    ASSERT(midi_out = jack_port_register(
               client, "out",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0));

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

