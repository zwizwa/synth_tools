// MIDI Clock code lifted from studio/c_src/jack_midi.c

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#include "pbuf.h"

/* AKAI FIRE */


/* JACK */
static jack_port_t *midi_out = NULL;
static jack_port_t *midi_in = NULL;
static jack_client_t *client = NULL;


#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

#define ROWS 4
#define COLS 16
uint8_t pads[ROWS][COLS] = {};

// Numbers are in hex, same as the doc
// https://blog.segger.com/decoding-the-akai-fire-part-1/
#define PAD_OFFSET 0x36

uint8_t pad_nb(uint8_t row, uint8_t col) {
    return PAD_OFFSET + col + row * 16;
}

#define SYSEX_HEADER_INIT { \
    0xF0, /* System Exclusive */ \
    0x47, /* Akai Manufacturer ID (see the MMA site for a list) */ \
    0x7F, /* The All-Call address */ \
    0x43, /* Sub-ID byte #1 identifies "Fire" product */ \
    0x65, /* Sub-ID byte #2 identifies the command */ \
}

const uint8_t sysex_header[] = SYSEX_HEADER_INIT;

#define SYSEX_FOOTER_BYTE 0xF7

void pad_event(void *out_buf, uint8_t row, uint8_t col) {
    pads[row][col] ^= 1;
    LOG("%d %d = %d\n", row, col, pads[row][col]);
    int on = pads[row][col];
    uint8_t data[] = {
        0x00, 0x04, // size in 7-7
        col + row * 16,   // pad nb
        on * 0x40, // r
        on * 0x40, // g
        on * 0x40, // b
    };
    int nb = sizeof(sysex_header) + sizeof(data) + 1;
    uint8_t *buf = jack_midi_event_reserve(out_buf, 0 /*time*/, nb);
    if (buf) {
        memcpy(buf, sysex_header, sizeof(sysex_header));
        buf += sizeof(sysex_header);
        memcpy(buf, data, sizeof(data));
        buf += sizeof(data);
        buf[0] = SYSEX_FOOTER_BYTE;
    }
    else {
        ERROR("Can't reserve %d midi bytes\n", nb);
    }
}


static int process (jack_nframes_t nframes, void *arg) {
    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
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
                if (note >= PAD_OFFSET) {
                    uint8_t col = note - PAD_OFFSET;
                    uint8_t row = col / 16;
                    col -= row * 16;
                    pad_event(midi_out_buf, row, col);
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

