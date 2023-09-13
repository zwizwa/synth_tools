/* Wrapper to start/stop Pd and provide a data channel on stdin.
   start: Set up Pd with netreceive and connect to it here.
   stop:  When stdin closes, ask Pd to shut down via netreceive channel */
#include "tcp_tools.h"
#include "assert_write.h"
#include "assert_read.h"
#include "macros.h"
#include "uct_byteswap.h"

int pd_fd = -1;

#define PD_WRITE(str) {                                 \
        const uint8_t buf[] = { str };                  \
        assert_write(pd_fd, buf, sizeof(buf)-1);        \
    }


static inline ssize_t erl_read(void *vbuf, size_t nb) {
    int fd = 0;
    unsigned char *buf = vbuf;
    if (nb == 0) return 0;
    ssize_t rv;
    do {
        rv = read(fd, buf, nb);
    } while(rv == -1 && errno == EINTR); // Haskell uses signals
    if (rv > 0) {
        //LOG("%2d: rv=%d\n", fd, rv);
    }
    else if (rv == 0) {
        /* Erlang side closed the pipe, which means we need to shut
           down.  Send a message to Pd then shut down this wrapper. */
        LOG("EOF on stdin. Sending shutdown to Pd.\n");
        PD_WRITE("shutdown;\r\n");
        close(pd_fd);
        exit(0);
    }
    else if (rv < 0) {
        int e = errno;
        LOG("fd %2d: errno=%d\n", fd, e);
    }
    ASSERT(rv > 0);
    return rv;
}
static inline ssize_t erl_read_fixed(void *vbuf, size_t nb) {
    unsigned char *buf = vbuf;
    size_t got = 0;
    while (got < nb) {
        ssize_t rv = erl_read(buf+got, nb-got);
        ASSERT(rv > 0);
        got += rv;
        // LOG("got=%d, rv=%d, nb=%d\n", got, rv, nb);
    }
    ASSERT_EQ(got, nb);
    return got;
}
static inline ssize_t erl_read_msg(void *vbuf) {
    uint8_t buf[4];
    erl_read_fixed(vbuf, 4);
    uint32_t len = read_be(buf, 4);
    return erl_read_fixed(vbuf, len);
}


int main(int argc, char **argv) {

    system("i.pd.exo");

    pd_fd = assert_tcp_connect("localhost", 3001);
    PD_WRITE("startup;\n");

    /* Start Pd in the background, open the exo patch. */
    for(;;) {
        uint8_t buf[1024]; // FIXME overflow
        erl_read_msg(buf);
    }
    return 0;
}
