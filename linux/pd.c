/* Wrapper to start/stop Pd and provide a data channel on stdin and an
   ALSA MIDI port.

   start: Set up Pd with netreceive and connect to it here.
   stop:  When stdin closes, ask Pd to shut down via netreceive channel

   Currently no need to export MIDI from Pd.

*/
#include <poll.h>

#include "tcp_tools.h"
#include "assert_write.h"
#include "assert_read.h"
#include "macros.h"
#include "uct_byteswap.h"
#include "alsa_tools.h"


#define MAX_MIDI 16


/* Erl */
int erl_fd = 0;

/* Pd */
int pd_fd = -1;
#define PD_WRITE(str) {                                 \
        const uint8_t buf[] = { str };                  \
        assert_write(pd_fd, buf, sizeof(buf)-1);        \
    }

/* ALSA */
snd_seq_t *seq;
snd_midi_event_t *alsa_decoder;
int alsa_client_id;
int alsa_port_id;

/* Erlang side closed the pipe, which means we need to shut down.
   Send a message to Pd then shut down this wrapper. */
static void eof_shutdown(void) {
    LOG("EOF on Erlang port (stdin)\n");
    LOG("Sending shutdown to Pd and exiting.\n");
    PD_WRITE("shutdown;\n");
    sleep(1);
    close(pd_fd);
    exit(0);
}

static inline ssize_t erl_read(void *vbuf, size_t nb) {
    unsigned char *buf = vbuf;
    if (nb == 0) return 0;
    ssize_t rv;
    do {
        rv = read(erl_fd, buf, nb);
    } while(rv == -1 && errno == EINTR); // Haskell uses signals
    if (rv > 0) {
        //LOG("%2d: rv=%d\n", fd, rv);
    }
    else if (rv == 0) {
        eof_shutdown();
    }
    else if (rv < 0) {
        int e = errno;
        LOG("fd %2d: errno=%d\n", erl_fd, e);
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

void handle_alsa(void) {
    do {
        snd_seq_event_t *ev;
        snd_seq_event_input(seq, &ev);
        // LOG("synth: event\n");
        switch(ev->type) {
        case SND_SEQ_EVENT_CLOCK:
            // Not needed until we have LFO sync.
            // LOG("event: clock\n");
            break;
        case SND_SEQ_EVENT_PORT_SUBSCRIBED:
            // LOG("event: port subscribed\n");
            break;
        default: {
            /* Not handling all snd_seq_event_type separately.
               Try to convert it to midi. */
            static unsigned char msg[MAX_MIDI];

            long n = snd_midi_event_decode(
                alsa_decoder, msg, sizeof(msg), ev);
            if (n == 1) {
                if (msg[0] == 0xF8) {
                }
                /* Convert some midi messages to PD messages. Just
                   write it to the socket.  This should not block in
                   realistic situations. */
                else if (msg[0] == 0xFA) {
                    PD_WRITE("start;\n");
                }
                else if (msg[0] == 0xFB) {
                    PD_WRITE("continue;\n");
                }
                else if (msg[0] == 0xFC) {
                    PD_WRITE("stop;\n");
                }
            }
            else if (n == 3) {
                /* Structure is optimized to make Pd route simple.
                   E.g. channel comes before message type. */
                uint8_t type = msg[0] & 0xF0;
                uint8_t chan = msg[0] & 0x0F;
                if (type == 0xB0) {
                    char fudi[32];
                    int nb = sprintf(fudi, "track %d cc %d %d;\n", chan, msg[1], msg[2]);
                    assert_write(pd_fd, (void*)fudi, nb);
                    // LOG("pd_io %s", msg);
                }
                else if ((type & 0xF0) == 0x80) {
                    char fudi[32];
                    /* Use 0 to mean off. */
                    int nb = sprintf(fudi, "track %d note %d %d;\n", chan, msg[1], 0);
                    assert_write(pd_fd, (void*)fudi, nb);
                }
                else if ((type & 0xF0) == 0x90) {
                    char fudi[32];
                    int nb = sprintf(fudi, "track %d note %d %d;\n", chan, msg[1], msg[2]);
                    assert_write(pd_fd, (void*)fudi, nb);
                }
            }
            else {
                /* Event not supported or decoder error. */
                LOG("WARNING: decode=%d, event=%d\n", n, ev->type);
            }
            break;
        }}
        snd_seq_free_event(ev);
    }
    while (snd_seq_event_input_pending(seq, 0) > 0);

}


int main(int argc, char **argv) {

    int rv;

    // FIXME: Keep track of the pid of the previous instance and kill
    // it here, just in case we lost connection and weren't able to
    // stop it.

    // Make sure previous Pd is terminated.
    rv = system("sleep .5");
    // Start a new one in the background.
    // FIXME: Move hardcoded path into SYNTH_TOOLS env var with default.
    rv = system("~/.result/synth_tools/pd/bin/pd exo.pd &");
    // Make sure the socket is up before we connect.
    rv = system("sleep 1");
    (void)rv;

    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));

    // const char *localhost = "localhost";
    const char *localhost = "127.0.0.1";
    pd_fd = assert_tcp_connect(localhost, 3001);


    PD_WRITE("startup;\n");

    /* ALSA */
    ALSA_ASSERT(snd_seq_open(&seq, "hw", SND_SEQ_OPEN_INPUT, 0));
    snd_seq_set_client_name(seq, "pd_io");
    alsa_client_id = snd_seq_client_id(seq);
    alsa_port_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            seq, "pd_io_midi",
            SND_SEQ_PORT_CAP_WRITE |
            SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_HARDWARE));
    ALSA_ASSERT(snd_midi_event_new(MAX_MIDI, &alsa_decoder));
    snd_midi_event_reset_decode(alsa_decoder);
    snd_midi_event_no_status(alsa_decoder, 1);
    int alsa_nfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    LOG("alsa alsa_nfd = %d\n", alsa_nfd);
    int extra_nfd = 1;
    int nfd = extra_nfd + alsa_nfd;
    struct pollfd pfd[nfd];
    snd_seq_poll_descriptors(seq, pfd + extra_nfd, alsa_nfd, POLLIN);

    /* Data coming from Erlang. */
    pfd[0].fd = erl_fd;
    pfd[0].events = POLLIN;

    /* Start Pd in the background, open the exo patch. */
    for(;;) {
        int rv;
        int timeout_ms = 1000;
        ASSERT_ERRNO(rv = poll(pfd, nfd, timeout_ms));

        /* Just bail on error. */
        for (int i=0; i<nfd; i++) {
            if (pfd[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                //LOG("pd_io shutdown\n");
                eof_shutdown();
            }
        }
        if (rv == 0) {
            /* Timeout */
            /* Send a message to make sure the connection is ok. */
            //LOG("pd_io -> pd: idle  (revents = 0x%x)\n", pfd.revents);
            PD_WRITE("idle;\n");
        }
        else if(pfd[0].revents & POLLIN) {
            uint8_t buf[1024]; // FIXME overflow
            /* FIXME: Add 2 protocols to uc_tools: Pd FUDI and framed MIDI */
            //LOG("pd_io read\n");
            erl_read_msg(buf);
        }
        else {
            handle_alsa();
        }
    }
    return 0;
}
