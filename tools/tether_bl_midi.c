#include "assert_write.h"

#define SEND(fd, ...) {                         \
        uint8_t midi[] = { __VA_ARGS__ };       \
        assert_write(fd, midi, sizeof(midi));   \
    }

void send_3if(int fd, const uint8_t *buf, uint32_t len) {
    SEND(fd, 0xF0 /* sysex start */, 0x12 /* manufacturer */);
    while(len > 0) {
        uint32_t n = (len > 7) ? 7 : len;
        uint8_t msbs = 0;
        for (uint32_t i=0; i<n; i++) {
            if (buf[i] & 0x80) {
                msbs |= (1 << i);
            }
        }
        assert_write(fd, &msbs, 1);
        assert_write(fd, buf, n);

        buf += n;
        len -= n;
    }
    SEND(fd, 0xF7);
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

    SEND_3IF(fd, 3, 10,20,30);

}

int main(int argc, char **argv) {
    const char *dev = "/dev/midi3";
    int fd;
    ASSERT_ERRNO(fd = open(dev, O_RDWR));
    test(fd);
    return 0;
}
