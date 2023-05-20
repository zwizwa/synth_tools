/* Platform-independent part of midi bootloader.*/

#include "mod_monitor.c"

#include <stdint.h>

#define BL_MIDI_SYSEX_MANUFACTURER 0x12

#ifndef BL_MIDI_LOG
#define BL_MIDI_LOG(...)
#endif

struct bl_state {
    void *next;
    struct {
        uint32_t nb_rx;
        uint32_t nb_rx_3if;
        uint32_t nb_rx_sysex;
    } stats;
    /* Decoder buffer for sysex.  The encoding used is: first byte
       contains high bits of subsequent 7 bytes (LSB is bit of first
       byte), then followed by up to 7 bytes containing the low 7
       bits.  That's simplest to decode. */
    uint8_t sysex_high_bits;
    uint8_t sysex_count;

};
struct bl_state bl_state;


/* I see 2 ways: do wrapping before writing to cbuf, or re-generate
   the wrapping here.  A sysex packet and a 3if packet do not need to
   correspond.  The 3if is self-delimiting stream.  Some constraints:

   - Include the F0 12 ... F7 framing in a single USB packet.  That
     makes the bookkeeping simpler.  This way there is room for a
     total of 48 7 bit bytes.  Minus 3 for the framing, that is (* 7
     (/ 45 8.0)) 39 bytes per 64 bytes of USB frame.

     This this is not simple.  There is packetizing, prefix, 8-to-7
     conversion, and we need to multiplex sysex streams also.  So
     maybe write the entire read operation as a protothread.

     Also the 64-byte bulk boundary needs to be respected.  How to do
     that?  That would be the main.

  -  Keep it simple: the 3if data can be chunked.  Later figure out how
     to incorporate app data as well.


*/


#include "sysex.h"

uint32_t monitor_read_sysex(uint8_t *buf, uint32_t room) {
    slice_uint8_t buf_slice = { .buf=buf, .len=room };
    sysex_stream_from_cbuf(BL_MIDI_SYSEX_MANUFACTURER,
                           &buf_slice, monitor.monitor_3if.out);
    return buf_slice.buf - buf;
}

uint32_t __attribute__((noinline)) usb_midi_read(uint8_t *buf, uint32_t room) {
    uint32_t nb = 0;
    /* Get high priority data first. */
    // FIXME

    /* Fill the rest with sysex. */
    nb += monitor_read_sysex(buf, room);
    return nb;
}


/* Here we need to distinguish between bootloader and app data, so
   parser goes here and should be as generic as possible. */
const uint8_t message_type_to_size[] = {
    /* Table 3-1 Packet Sizes based on Message Types
       The length is in multiples of 32 bit. */

    [0x0] = 1,  // Utility Messages
    [0x1] = 1,  // System Real Time and System Common Messdages (except System Exclusive)
    [0x2] = 1,  // MIDI 1.0 Channel Voice Messages
    [0x3] = 2,  // Data Messages (including System Exclusive)
    [0x4] = 2,  // MIDI 2.0 Channel Voice Messages
    [0x5] = 3,  // Data Messages

    /* All the rest is Reserved */
    [0x6] = 1,
    [0x7] = 1,
    [0x8] = 2,
    [0x9] = 2,
    [0xa] = 1,
    [0xb] = 3,
    [0xc] = 3,
    [0xd] = 4,
    [0xe] = 4,
    [0xf] = 4,
};

/* How to map this?  I will probably want to have both UART midi and
   this USB midi.  So not sure how to split it up.  What I want:

   - The other end should receive complete messaages

   - Sysex should use a stream interface.  Conceptually this is a
     separate channel.

   Note that USB MIDI uses Universal MIDI Packet format (UMP), which
   is defined elsewhere.

*/

/* MIDI 1.0 was simple. MIDI 2.0 is not. I don't feel like reading all
   the documentation, so I just test by writing to /dev/midiX and see
   what arrives here.  The "old school" approach of treating midi as 2
   different streams (low-priority sysex and everything else
   pre-empting that) seems to be ok, so that is what we provide to the
   app. */

/* The first byte after 0xF0 is a dispatch code.  I'm going to use
   0x12 which seems to be a defunct company. Change it later if
   needed.

   https://www.midi.org/specifications/midi-reference-tables/manufacturer-sysex-id-numbers
*/


void bl_app_push(struct bl_state *s, uint8_t byte) {
}

#define BL_MIDI_SYSEX_NEXT_LABEL(s,label) {              \
        (s)->next = &&label; return; label:{}            \
    }
#define BL_MIDI_SYSEX_NEXT(s)                   \
    BL_MIDI_SYSEX_NEXT_LABEL(s,GENSYM(label_))


void __attribute__((noinline)) bl_3if_push(struct bl_state *s, uint8_t byte) {
    s->stats.nb_rx_3if++;
    BL_MIDI_LOG(s, "3if_push: %02x\n", byte);
    monitor_write(&byte, 1);
}

void bl_midi_sysex_push(struct bl_state *s, uint8_t byte) {

    BL_MIDI_LOG(s, "sysex_push: %02x\n", byte);

    if (byte == 0xF0) goto packet_start;
    if (unlikely(!s->next)) return;
    goto *s->next;

  packet:
    BL_MIDI_SYSEX_NEXT(s);
    if (byte != 0xF0) {
        /* Ignore everything up to the sysex start byte. */
        goto packet;
    }
  packet_start:
    /* byte == 0x0F0 sysex start */
    /* First byte after start byte is manufacturer. */
    BL_MIDI_SYSEX_NEXT(s);
    if (byte == BL_MIDI_SYSEX_MANUFACTURER) {
        /* 3IF is addressed.  Perform conversion to 8-bit data and
           push it into the interpreter. */
        for (;;) {
            BL_MIDI_SYSEX_NEXT(s);
            if (byte == 0xF7) goto packet;
            s->sysex_high_bits = byte;
            for (s->sysex_count = 0;
                 s->sysex_count < 7;
                 s->sysex_count++) {
                BL_MIDI_SYSEX_NEXT(s);
                if (byte == 0xF7) goto packet;
                if (1 & (s->sysex_high_bits >> s->sysex_count)) {
                    byte |= 0x80;
                }
                bl_3if_push(s, byte);
            }
        }
    }
    /* Everything else goes to the application. */
    bl_app_push(s, 0xF0);
    for(;;) {
        bl_app_push(s, byte);
        if (byte == 0xF7) goto packet;
        BL_MIDI_SYSEX_NEXT(s);
    }
}



void bl_midi_write_sysex(struct bl_state *s, const uint8_t *buf, uint32_t len) {
    for (uint32_t i=0; i< len; i++) {
        bl_midi_sysex_push(s, buf[i]);
    }
}

void __attribute__((noinline)) bl_midi_write(struct bl_state *s, const uint8_t *buf, uint32_t len) {
    s->stats.nb_rx += len;
    while(len > 0) {
        /* See UMP spec appendix G, All defined messages. */
        uint8_t msg0 = buf[0];
        uint8_t message_type = (msg0 >> 4) & 0xF;
        uint8_t group = msg0 & 0xF;
        uint8_t message_size = 4 * message_type_to_size[message_type];
        switch(group) {
        /* Figure out where these group tags are specified.  These
           have just been reverse engineered from what Linux sends to
           the USB endpoint. */
        case 0x7:
        case 0x4: bl_midi_write_sysex(s, &buf[1], 3); break;
        case 0x5: bl_midi_write_sysex(s, &buf[1], 1); break;
        case 0x6: bl_midi_write_sysex(s, &buf[1], 2); break;
        case 0x8: // Note off
        case 0x9: // Note on
            break;
        }
        buf += message_size;
        len -= message_size;
    }
}
void __attribute__((noinline)) usb_midi_write(const uint8_t *buf, uint32_t len) {
    bl_midi_write(&bl_state, buf, len);
}


