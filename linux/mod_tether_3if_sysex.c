/* Host end of 3if over sysex */

#ifndef MOD_TETHER_3IF_SYSEX
#define MOD_TETHER_3IF_SYSEX

#ifndef TETHER_3IF_LOG_DBG
#define TETHER_3IF_LOG_DBG(...)
#endif

#include "mod_tether_3if.c"
#include "sysex.h"

void tether_sysex_write(struct tether *s, const uint8_t *buf, size_t len) {
    uint32_t nb_data = sysex_encode_8bit_to_7bit_needed(len);
    uint8_t sysex[3 + nb_data];
    sysex[0] = 0xF0;
    sysex[1] = 0x12;
    const_slice_uint8_t in = { .buf = buf, .len = len };
    sysex_encode_8bit_to_7bit(sysex + 2, &in);
    sysex[2 + nb_data] = 0xF7;
    for(uint32_t i=0; i<sizeof(sysex); i++) {
        TETHER_3IF_LOG_DBG(" %02x", sysex[i]);
    }
    TETHER_3IF_LOG_DBG("\n");

    /* Use a single write call to allow pipe2 O_DIRECT packet mode. */
    assert_write(s->fd_out, sysex, sizeof(sysex));
}

struct tether_sysex {
    struct tether tether;
    // Size is 3 bytes framing, then 8 per 7 for encoding of a packet.
    // This is overdimensioned.
    void *next;
    uint8_t msbs;
    uint8_t count;
};


/* Use a coroutine that produces a single byte at a time, blocking on
   the fd read of the sysex data.  This is the same structure as the
   mod_bl_midi.c code, just different blocking points. */

#define TETHER_SYSEX_PUT_LABEL(s,byte,label) {              \
        (s)->next = &&label; return byte; label:{}          \
    }
#define TETHER_SYSEX_PUT(s,byte) {                      \
        TETHER_SYSEX_PUT_LABEL(s,byte,GENSYM(label_))   \
}

/* FIXME: Add buffering, so it is compatible with O_DIRECT read. */
uint8_t tether_sysex_get_byte(struct tether_sysex *s) {
    uint8_t byte;
    assert_read_fixed(s->tether.fd_in, &byte, 1);
    // LOG("get %02x\n", byte);
    return byte;
}

uint8_t tether_sysex_get(struct tether_sysex *s) {
    uint8_t byte;
    if (s->next) goto *s->next;
  next_packet:
    do { byte = tether_sysex_get_byte(s); } while(byte != 0xF0);
    byte = tether_sysex_get_byte(s);
    if (byte == 0xF7) { goto next_packet; }
    if (byte != 0x12) {
        ERROR("0x%02x != 0x12\n", byte);
    }
    for(;;) {
        byte = tether_sysex_get_byte(s);
        if (byte == 0xF7) { goto next_packet; }
        s->msbs = byte;
        for(s->count = 0; s->count < 7; s->count++) {
            byte = tether_sysex_get_byte(s);
            if (byte == 0xF7) { goto next_packet; }
            if (s->msbs & (1 << s->count)) { byte |= 0x80; }
            TETHER_SYSEX_PUT(s, byte);
        }
    }
}
ssize_t tether_sysex_read(struct tether *s_, void *vbuf, size_t nb) {
    struct tether_sysex *s = (void*)s_;
    uint8_t *buf = vbuf;
    for (size_t i=0; i<nb; i++) {
        buf[i] = tether_sysex_get(s);
    }
    return nb;
}

void tether_set_midi_fds(struct tether_sysex *s, int fd_in, int fd_out) {
    s->tether.fd_in = fd_in;
    s->tether.fd_out = fd_out;
    // FIXME: This should probably use an explicit init.
    if (!s->tether.read)  { s->tether.read  = tether_sysex_read; }
    if (!s->tether.write) { s->tether.write = tether_sysex_write; }
}


void tether_open_midi(struct tether_sysex *s, const char *dev) {
    int fd;
    ASSERT_ERRNO(fd = open(dev, O_RDWR));
    tether_set_midi_fds(s, fd, fd);
}


#endif
