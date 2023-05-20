#include "assert_write.h"

#define SEND(fd, ...) {                         \
        uint8_t midi[] = { __VA_ARGS__ };       \
        assert_write(fd, midi, sizeof(midi));   \
    }

#include "sysex.h"

void send_3if(int fd, const uint8_t *buf, uint32_t len) {
    const_slice_uint8_t in = { .buf = buf, .len = len };

    uint8_t out_buf[len * 2];
    uint8_t *out = out_buf;

    *out++ = 0xF0; /* sysex start */
    *out++ = 0x12; /* manufacturer */
    while(in.len > 0) {
        uintptr_t nb = sysex_encode_8bit_to_7bit(out, &in);
        out += nb;
    }
    *out++ = 0xF7;

    uintptr_t n_out = out - out_buf;
    for(uint32_t i=0; i<n_out; i++) { LOG(" %02x", out_buf[i]); } LOG("\n");

    assert_write(fd, out_buf, n_out);
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

    SEND_3IF(fd, 3, 201,202,203);
    //SEND_3IF(fd, 3, 10,20,30);

}

int main(int argc, char **argv) {
    const char *dev = "/dev/midi3";
    int fd;
    ASSERT_ERRNO(fd = open(dev, O_RDWR));
    test(fd);
    return 0;
}
