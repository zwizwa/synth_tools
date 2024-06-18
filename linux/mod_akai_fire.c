#ifndef MOD_AKAI_FIRE
#define MOD_AKAI_FIRE

#include "macros.h"
#include "pbuf.h"
#include "cbuf.h"
#include <stdint.h>


struct mmc;
void mmc_press_stop(struct mmc *mmc);
void mmc_press_play(struct mmc *mmc);
void mmc_press_record(struct mmc *mmc);

struct route;
void route_cc  (struct route *route, uintptr_t sel, uint8_t ctrl, uint8_t val);
void route_note(struct route *route, uintptr_t sel, uint8_t on_off, uint8_t note, uint8_t vel);
void route_raw_midi(struct route *route, uint8_t sel,
                    const uint8_t *buf, uintptr_t size);


void to_erl_midi(const uint8_t *buf, int nb, uint8_t port);
void to_erl_pterm(const char *pterm);


/* AKAI FIRE */
#define AKAI_FIRE_ROWS 4
#define AKAI_FIRE_COLS 16
struct akai_fire;
struct akai_fire {
    void (*button_notify)(struct akai_fire *, int row, int col);
    uint8_t pads[AKAI_FIRE_ROWS][AKAI_FIRE_COLS];
    uint32_t need_update:1;
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

void akai_fire_sysex_buttons(struct akai_fire *fire,
                             struct route *route,
                             int ncols, int nrows) {
    /* pbuf to build up the sysex message incrementally */
    struct pbuf p = {
        .size =
        sizeof(akai_fire_sysex_header) +
        2 /* size in 7-7 */ +
        (4 * ncols * nrows) +
        sizeof(akai_fire_sysex_footer)
    };
    uint8_t p_buf[p.size];
    p.buf = p_buf;
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

    route_raw_midi(route, sel_fire, p.buf, p.count);
}
void akai_fire_sysex_buttons_all(struct akai_fire *fire, void *out_buf) {
    akai_fire_sysex_buttons(fire, out_buf, AKAI_FIRE_COLS, AKAI_FIRE_ROWS);
}

void akai_fire_pad_update(struct akai_fire *fire, struct route *route) {
    if (!fire->need_update) return;
    LOG("akai_fire: need update\n");
    fire->need_update = 0;

    if (1) {
        akai_fire_sysex_buttons_all(fire, route);
        // akai_fire_sysex_buttons(fire, out_buf, 1, 4);
    }

    if (0) {
        route_cc(route, sel_fire,  0x33 /* play button */, 0x04 /* hi green */);
    }
}
void akai_fire_pad_event(struct akai_fire *fire, uint8_t row, uint8_t col) {
    if (fire->button_notify) {
        /* Handler is supposed to update the state and set need_update. */
        fire->button_notify(fire, row, col);
    }
    else {
        fire->pads[row][col] ^= 1;
        fire->need_update = 1;
    }
    LOG("button %d %d = %d\n", row, col, fire->pads[row][col]);
}

static inline void process_akai_fire(
    /* Private state data */
    struct akai_fire *fire,
    /* Stateful local objects. */
    struct mmc *mmc,
    struct sequencer *seq,
    /* Remote uni-directional message targets. */
    struct route *route,
    /* Midi data */
    const uint8_t *msg, int n) {

    if (n == 3) {
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

    akai_fire_pad_update(fire, route);

}

void akai_fire_init(struct akai_fire *fire) {
    memset(fire,0,sizeof(*fire));
    fire->need_update = 1;
}

#endif
