/* stdio to Jack MIDI bridge

   Originally written to allow tether_bl_midi.c (which was written for
   /dev/midiX) to talk to a jack device as well.  There was the idea
   to do the bridge in Erlang but it seems best to just have a C
   bridging application which can then be run via socat etc...

   The reason to not put the TCP daemon here is because I don't want
   to deal with managing multiple connections.  Keep the semantics of
   the C file clean.

   Note that this should probably be generalized: all these clients
   should either have the Erlang jack_client.erl protocol on stdio, or
   just plain midi like this one.

*/

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "jack_tools.h"
#include "assert_read.h"
#include "assert_write.h"
#include "midi_frame.h"



/* JACK */
#define FOR_MIDI_IN(m) \
    m(midi_in)         \

#define FOR_MIDI_OUT(m) \
    m(midi_out)         \

FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

static inline void *midi_out_buf(jack_port_t *port, jack_nframes_t nframes) {
    void *buf = jack_port_get_buffer(port, nframes);
    jack_midi_clear_buffer(buf);
    return buf;
}
// Send midi data out over a jack port.
static inline void send_midi(void *out_buf, jack_nframes_t time,
                             const void *data_buf, size_t nb_bytes) {
    //LOG("%d %d %d\n", frames, time, (int)nb_bytes);
    void *buf = jack_midi_event_reserve(out_buf, time, nb_bytes);
    if (buf) memcpy(buf, data_buf, nb_bytes);
}
static inline void send_cc(void *out_buf, int chan, int cc, int val) {
    const uint8_t midi[] = {0xB0 + (chan & 0x0F), cc & 0x7F, val & 0x7F};
    send_midi(out_buf, 0, midi, sizeof(midi));
}


/* Erlang */
#define TO_TCP_SIZE_LOG 12
#define TO_TCP_SIZE (1 << TO_TCP_SIZE_LOG)
static uint8_t to_tcp_buf[TO_TCP_SIZE];
static size_t to_tcp_buf_bytes = 0;
static uint8_t *to_tcp_hole(int msg_size) {
    if (to_tcp_buf_bytes + msg_size > sizeof(to_tcp_buf)) {
        LOG("midi buffer overflow\n");
        return NULL;
    }
    uint8_t *msg = &to_tcp_buf[to_tcp_buf_bytes];
    to_tcp_buf_bytes += msg_size;
    return &msg[0];
}
static void to_tcp(const uint8_t *buf, int nb) {
    uint8_t *hole = to_tcp_hole(nb);
    if (hole) { memcpy(hole, buf, nb); }
}



static inline void process_midi_in(jack_nframes_t nframes, uint8_t stamp) {
    FOR_MIDI_EVENTS(iter, midi_in, nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        to_tcp(msg, n);
#if 0
        if (n == 3) {
            if ((msg[0] & 0xF0) == 0xB0) { // CC
                uint8_t cc  = msg[1];
                uint8_t val = msg[2];
                // FIXME: Route to pd etc...
            }
        }
#endif
    }
}




static inline void process_tcp_out(jack_nframes_t nframes) {
    /* Send to Erlang

       Note: I'm not exactly sure whether it is a good idea to perform
       the write() call from this thread, but it seems the difference
       between a single semaphore system call and a single write to an
       Erlang port pipe accessing a single page of memory is not going
       to be big.  So revisit if it ever becomes a problem.

       This will buffer all midi messages and perform only a single
       write() call.

    */

    if (to_tcp_buf_bytes) {
        //LOG("buf_bytes = %d\n", (int)to_tcp_buf_bytes);
        assert_write(1, to_tcp_buf, to_tcp_buf_bytes);
        to_tcp_buf_bytes = 0;
    }

}

static int process(jack_nframes_t nframes, void *arg) {

    /* Erlang out is tagged with a rolling time stamp. */
    jack_nframes_t f = jack_last_frame_time(client);
    uint8_t stamp = (f / nframes);

    /* Order is important. */
    process_midi_in(nframes, stamp);
    process_tcp_out(nframes);

    return 0;
}

void midi(struct midi_frame *f) {
}

int main(int argc, char **argv) {

    /* Jack client setup */
    if (argc < 2) {
        LOG("usage: %s <client_name>\n", argv[0]);
        exit(1);
    }
    const char *client_name = argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);
    FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
    FOR_MIDI_OUT(REGISTER_JACK_MIDI_OUT);

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* Parse MIDI on stdin */
    struct midi_frame f;
    midi_frame_init(&f, midi);
    for(;;) {
        uint8_t buf[1024];
        ssize_t nb_read = read(0, buf, sizeof(buf));
        ASSERT(nb_read > 0);
        midi_frame_push_buf(&f, buf, nb_read);
    }

    return 0;
}

