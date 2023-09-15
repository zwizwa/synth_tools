/* Wrapper to start/stop Pd and provide a data channel on stdin.
   start: Set up Pd with netreceive and connect to it here.
   stop:  When stdin closes, ask Pd to shut down via netreceive channel

   Also provides a MIDI to audio bridge.  Mostly done to bring
   accurate sync into Pd by keeping the signal inside jack.

   Currently no need to export MIDI from Pd.

*/
#include <jack/jack.h>
#include <jack/midiport.h>
#include "tcp_tools.h"
#include "assert_write.h"
#include "assert_read.h"
#include "macros.h"
#include "uct_byteswap.h"


/* Jack */
static jack_port_t *audio_out = NULL;
static jack_port_t *midi_in = NULL;
static jack_client_t *client = NULL;

int nb_clock = 0;

static inline void process_midi(jack_nframes_t nframes) {
    void *midi_in_buf  = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
    for (jack_nframes_t i = 0; i < n; i++) {
        jack_midi_event_t event;
        jack_midi_event_get(&event, midi_in_buf, i);
        const uint8_t *msg = event.buffer;
        if (event.size == 1 &&
            msg[0] == 0xF8) {
            nb_clock++;
        }

#if 0 // FIXME: Later, maybe convert control messages to FUDI
        else if (event.size == 3 &&
            msg[0] == 0xB0 && // CC channel 0
            (msg[1] >= 23) && // CC num on Easycontrol 9
            (msg[1] <= 31)) {
            // ...
        }
        else if (event.size == 3 &&
                 msg[0] == 0x90) { // Note on channel 0
            uint8_t note = msg[1];
            uint8_t vel  = msg[2];
            if (vel == 0) {
                synth_note_off(&synth, note);
            }
            else {
                synth_note_on(&synth, note);
            }
        }
        else if (event.size == 3 &&
                 msg[0] == 0x80) { // Note off channel 0
            uint8_t note = msg[1];
            // uint8_t vel  = msg[2];
            synth_note_off(&synth, note);
        }
#endif
    }
}


static inline void process_audio(jack_nframes_t nframes) {
    //LOG("\raudio %d ", count++);
    jack_default_audio_sample_t *dst =
        jack_port_get_buffer(audio_out, nframes);
    /* Sync is 24 per quarter note (beat), so 120bpm is 2 bps is 48
       pulses per second. */

    /* No pulses needs to be handled separately due to division used
       for hperiod. */
    if (nb_clock == 0) {
        for (int t=0; t<nframes; t++) {
            dst[t] = 0;
        }
        return;
    }

    /* It's ok to just use integer math here, which will cause the
       last segment to be slightly longer in some cases. We don't care
       much about jitter. */
    jack_nframes_t nsegments = 2 * nb_clock;
    nb_clock = 0;

    float hperiod = ((float)nframes) / ((float)nsegments);
    float offset = 0;
    int phase = 1;
    for (int t=0; t<nframes; t++) {
        if (t >= offset) {
            offset += hperiod;
            phase ^= 1;
        }
        dst[t] = phase;
    }
}

static int process (jack_nframes_t nframes, void *arg) {
    /* Order is important. */
    process_midi(nframes);
    process_audio(nframes);
    return 0;
}


/* Pd */
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

    int rv = system("i.pd.exo");
    (void)rv;

    /* Pd midi is problematic, so solve it in this adapter. */
    const char *client_name = "pd_io";

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    ASSERT(midi_in = jack_port_register(
               client, "in",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));

    ASSERT(audio_out = jack_port_register(
               client, "out",
               JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));


    pd_fd = assert_tcp_connect("localhost", 3001);
    PD_WRITE("startup;\n");

    /* Start Pd in the background, open the exo patch. */
    for(;;) {
        uint8_t buf[1024]; // FIXME overflow
        /* FIXME: Add 2 protocols to uc_tools: Pd FUDI and framed MIDI */
        erl_read_msg(buf);
    }
    return 0;
}
