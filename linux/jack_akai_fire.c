// MIDI Clock code lifted from studio/c_src/jack_midi.c

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#include "pbuf.h"

/* AKAI FIRE */
int need_update;


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

const uint8_t sysex_header[] = {
    0xF0, /* System Exclusive */
    0x47, /* Akai Manufacturer ID (see the MMA site for a list) */
    0x7F, /* The All-Call address */
    0x43, /* Sub-ID byte #1 identifies "Fire" product */
    0x65, /* Sub-ID byte #2 identifies the command */
};
const uint8_t sysex_footer[] = {
    0xF7
};

// Odd: writing one byte at a time the controller seems to crash after
// all pads have been toched.

void pad_update(void *out_buf) {
    if (!need_update) return;
    need_update = 0;

    struct pbuf p = { .size = sizeof(sysex_header) + 2 + 256 + 1 };
    p.buf = jack_midi_event_reserve(out_buf, 0 /*time*/, p.size);
    if(!p.buf) {
        ERROR("Can't reserve %d midi bytes\n", p.size);
    }
    pbuf_write(&p, sysex_header, sizeof(sysex_header));
    uint8_t size_hdr[] = {0x02, 0x00 /* size in 7-7 */};
    pbuf_write(&p, size_hdr, sizeof(size_hdr));
    for(int row=0; row<4; row++) {
        for(int col=0; col<16; col++) {
            int nb = col + 16 * row;
            int on = pads[row][col];
            uint8_t frame[] = {
                nb,
                on * 0x40, // r
                on * 0x40, // g
                on * 0x40, // b
            };
            pbuf_write(&p, frame, sizeof(frame));
        }
    };
    pbuf_write(&p, sysex_footer, sizeof(sysex_footer));
}
void pad_event(uint8_t row, uint8_t col) {
    pads[row][col] ^= 1;
    LOG("%d %d = %d\n", row, col, pads[row][col]);
    need_update = 1;
}


static int process (jack_nframes_t nframes, void *arg) {
    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
    jack_midi_clear_buffer(midi_out_buf);

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
                    pad_event(row, col);
                }
            }
            else if (msg[0] == 0x80) { // Note off channel 0
                // sprintf(buf, "off %d %d;\n", msg[1], msg[2]);
            }
        }
        else {
        }
    }

    pad_update(midi_out_buf);

    return 0;
}


int main(int argc, char **argv) {

    need_update = 1;

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

