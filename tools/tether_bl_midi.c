#include "assert_write.h"

#define SEND(fd, ...) {                         \
    uint8_t midi[] = { __VA_ARGS__ };           \
    assert_write(fd, midi, sizeof(midi));       \
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


#if 1
    SEND(fd,
         0xf0,
         1,
         2,
         0xf7);
    // Yields two packets:
    //   04 F0 01 02
    //   05 F7 00 00

#endif

}

int main(int argc, char **argv) {
    const char *dev = "/dev/midi3";
    int fd;
    ASSERT_ERRNO(fd = open(dev, O_RDWR));
    test(fd);
    return 0;
}
