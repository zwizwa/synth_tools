/* Generic MIDI framing code.  Takes a MIDI byte stream as input and
   invokes a callback for each complete message.  This should be
   designed for the lowest level, to be run from microcontroller UART
   ISR. */

#ifndef MIDI_FRAME_H
#define MIDI_FRAME_H

#include "pbuf.h"

struct midi_frame;
typedef void (*midi_frame_handle_fn)(struct midi_frame *);
struct midi_frame {
    midi_frame_handle_fn handle;
    struct pbuf p;
    uint8_t p_buf[4];
};

static inline void midi_frame_init(struct midi_frame *f,
                                   midi_frame_handle_fn handle) {
    memset(f, 0, sizeof(*f));
    PBUF_INIT(f->p);
    f->handle = handle;
}

/* Push byte and call f->handle when frame is done. */
static inline void midi_frame_push(struct midi_frame *f, uint8_t byte) {
    // FIXME: Switch to sysex mode.
    pbuf_write(&f->p, &byte, 1);
    switch(f->p.buf[0] >> 4) {
    case 0x8:
    case 0x9:
    case 0xA:
    case 0xB:
        if (f->p.count == 3) {
            f->handle(f);
            pbuf_clear(&f->p);
        }
    }
}

static inline void midi_frame_push_buf(struct midi_frame *f,
                                       const uint8_t *buf, int n) {
    for (int i = 0; i<n; i++) {
        midi_frame_push(f, buf[i]);
    }
}

#endif
