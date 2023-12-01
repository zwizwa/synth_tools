#ifndef MOD_AKAI_FIRE
#define MOD_AKAI_FIRE

/* Depends on jack midi. */

#include "macros.h"
#include "pbuf.h"
#include "cbuf.h"
#include <stdint.h>
#include <jack/jack.h>
#include <jack/midiport.h>

/* AKAI FIRE */
#define AKAI_FIRE_ROWS 4
#define AKAI_FIRE_COLS 16
struct akai_fire {
    int need_update;
    uint8_t pads[AKAI_FIRE_ROWS][AKAI_FIRE_COLS];
};


// Numbers are in hex, same as the doc
// https://blog.segger.com/decoding-the-akai-fire-part-1/
#define PAD_OFFSET 0x36

//uint8_t pad_nb(uint8_t row, uint8_t col) {
//    return PAD_OFFSET + col + row * 16;
//}

const uint8_t akai_fire_sysex_header[] = {
    0xF0, /* System Exclusive */
    0x47, /* Akai Manufacturer ID (see the MMA site for a list) */
    0x7F, /* The All-Call address */
    0x43, /* Sub-ID byte #1 identifies "Fire" product */
    0x65, /* Sub-ID byte #2 identifies the command */
};
const uint8_t akai_fire_sysex_footer[] = {
    0xF7
};

// Odd: writing one byte at a time the controller seems to crash after
// all pads have been toched.

void akai_fire_sysex_buttons(struct akai_fire *fire, void *out_buf,
                             int ncols, int nrows) {
    struct pbuf p = {
        .size =
        sizeof(akai_fire_sysex_header) +
        2 /* size in 7-7 */ +
        (4 * ncols * nrows) +
        sizeof(akai_fire_sysex_footer)
    };
    p.buf = jack_midi_event_reserve(out_buf, 0 /*time*/, p.size);
    if(!p.buf) {
        ERROR("Can't reserve %d midi bytes\n", p.size);
    }
    pbuf_write(&p, akai_fire_sysex_header, sizeof(akai_fire_sysex_header));
    int size = 4 * ncols * nrows;
    uint8_t size_hdr[] = {
        /* size in 7-7 */
        size >> 7,
        size & 0x7ff
    };
    pbuf_write(&p, size_hdr, sizeof(size_hdr));
    for(int row=0; row<nrows; row++) {
        for(int col=0; col<ncols; col++) {
            int nb = col + 16 * row;
            int on = fire->pads[row][col];
            uint8_t frame[] = {
                nb,
                on * 0x40, // r
                on * 0x40, // g
                on * 0x40, // b
            };
            pbuf_write(&p, frame, sizeof(frame));
        }
    };
    pbuf_write(&p, akai_fire_sysex_footer, sizeof(akai_fire_sysex_footer));
    LOG("akai_fire: update wrote %d bytes, expected %d\n", p.count, p.size);
    LOG_HEX("sysex:",p.buf,p.count);
}
void akai_fire_sysex_buttons_all(struct akai_fire *fire, void *out_buf) {
    akai_fire_sysex_buttons(fire, out_buf, AKAI_FIRE_COLS, AKAI_FIRE_ROWS);
}

void akai_fire_pad_update(struct akai_fire *fire, void *out_buf) {
    if (!fire->need_update) return;
    LOG("akai_fire: need update\n");
    fire->need_update = 0;

    if (1) {
        akai_fire_sysex_buttons_all(fire, out_buf);
        // akai_fire_sysex_buttons(fire, out_buf, 1, 4);
    }

    if (1) {
        const uint8_t testmsg[] = { 0xB0, 0x33 /* play button */, 0x04 /* hig green */};
        uint8_t *buf = jack_midi_event_reserve(out_buf, 0 /*time*/, sizeof(testmsg));
        ASSERT(buf);
        memcpy(buf, testmsg, sizeof(testmsg));
    }
}
void akai_fire_pad_event(struct akai_fire *fire, uint8_t row, uint8_t col) {
    fire->pads[row][col] ^= 1;
    LOG("%d %d = %d\n", row, col, fire->pads[row][col]);
    fire->need_update = 1;
}

void akai_fire_process(struct akai_fire *fire,
                       void *midi_out_buf,
                       void *midi_in_buf) {
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {

        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        const uint8_t *msg = event.buffer;

        LOG_HEX("akai_fire:", msg, event.size);

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
                    akai_fire_pad_event(fire, row, col);
                }
            }
            else if (msg[0] == 0x80) { // Note off channel 0
                // sprintf(buf, "off %d %d;\n", msg[1], msg[2]);
            }
        }
        else {
        }
    }

    akai_fire_pad_update(fire, midi_out_buf);

}

#if 0 // FIXME: clean up all the C struct inits
void akai_fire_init(struct akai_fire *fire) {
    memset(fire,0,sizeof(*fire));
    fire->need_update = 1;
}
#endif

#endif
