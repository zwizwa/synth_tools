
#include "mod_tether_3if_sysex.c"


#if 0 // bootstrapping code, can be removed



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

    struct tether_sysex t = { .tether = { .fd = fd } };
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

#endif

int main(int argc, char **argv) {
    const char *dev = "/dev/midi3";
    struct tether_sysex s = {};
    tether_open_midi(&s, dev);

    struct tether *t = &s.tether;

    uint8_t buf[1000] = {};
    tether_read_mem(t, buf, 0x08000000, sizeof(buf), LDF, NFL);

    for (uint32_t i=0; i<sizeof(buf); i++) { LOG(" %02x", buf[i]); }
    LOG(" (%d)\n", sizeof(buf));


    //test(fd);
    //test(fd);
    return 0;
}
