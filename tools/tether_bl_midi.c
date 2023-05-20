
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

    for(uint32_t i=0; i<sizeof(sysex); i++) { LOG(" %02x", sysex[i]); } LOG("\n");

    assert_write(s->fd, sysex, sizeof(sysex));
}

struct tether_midi {
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
#define TETHER_SYSEX_PUT(s,byte)                        \
    TETHER_SYSEX_PUT_LABEL(s,byte,GENSYM(label_))

uint8_t tether_sysex_get(struct tether_midi *s) {
    uint8_t byte;
    if (s->next) goto *s->next;
  next_packet:
    do assert_read_fixed(s->tether.fd, &byte, 1); while(byte != 0xF0);
    assert_read_fixed(s->tether.fd, &byte, 1);
    if (byte == 0xF7) { goto next_packet; }
    ASSERT(byte == 0x12);
    for(;;) {
        assert_read_fixed(s->tether.fd, &byte, 1);
        if (byte == 0xF7) { goto next_packet; }
        s->msbs = byte;
        for(s->count = 0; s->count < 7; s->count++) {
            assert_read_fixed(s->tether.fd, &byte, 1);
            if (byte == 0xF7) { goto next_packet; }
            if (s->msbs & (1 << s->count)) { byte |= 0x80; }
            TETHER_SYSEX_PUT(s, byte);
        }
    }
}


/* Read needs to be buffered, i.e. read one sysex packet at a time. */
ssize_t tether_sysex_read(struct tether *s_, void *vbuf, size_t nb) {
    struct tether_midi *s = (void*)s_;
    uint8_t *buf = vbuf;
    for (size_t i=0; i<nb; i++) {
        buf[i] = tether_sysex_get(s);
    }
    return nb;
}

#define SEND(fd, ...) {                         \
        uint8_t midi[] = { __VA_ARGS__ };       \
        assert_write(fd, midi, sizeof(midi));   \
    }


void send_3if(int fd, const uint8_t *buf, uint32_t len) {
    struct tether s = { .fd = fd };
    tether_sysex_write(&s, buf, len);

}

#define SEND_3IF(fd, ...) {                    \
        uint8_t buf[] = { __VA_ARGS__ };       \
        send_3if(fd, buf, sizeof(buf));        \
    }

void test(int fd) {
#if 0
    SEND(fd,
         0x90, // Note on channel 1
         64,   // note number
         126); // velocity
#endif

#if 0
    SEND(fd,
         0xf0,
         1,
         2,
         3,
         0xf7);
    // Yields two packets:
    //   04 F0 01 02
    //   06 03 F7 00
    //
#endif


#if 0
    SEND(fd,
         0xf0,
         1,
         2,
         0xf7);
    // Yields two packets:
    //   04 F0 01 02
    //   05 F7 00 00

#endif

#if 0
    SEND(fd,
         0xf0,
         0x12,  // Manufacturer code
         0,     // byte with MSBs
         0,     // LSBs of first byte
         0xf7);

#endif


#define NPUSH 0x81
    SEND_3IF(fd, 4, NPUSH, 1, 2, 3);

    struct tether_midi t = { .tether = { .fd = fd } };
    uint8_t resp_buf[2];
    tether_sysex_read(&t.tether, resp_buf, sizeof(resp_buf));
    for (uint32_t i=0; i<sizeof(resp_buf); i++) { LOG(" %02x", resp_buf[i]); }
    LOG(" (%d)\n", sizeof(resp_buf));

#if 0
    for(;;) {
        uint8_t byte = 0;
        assert_read_fixed(fd, &byte, 1);
        LOG(" %02x\n", byte);
    }
#endif
    

    //SEND_3IF(fd, 3, 10,20,30);

}

int main(int argc, char **argv) {
    const char *dev = "/dev/midi3";
    int fd;
    ASSERT_ERRNO(fd = open(dev, O_RDWR));
    test(fd);
    return 0;
}
