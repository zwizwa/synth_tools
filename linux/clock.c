/* MIDI Clock code lifted from studio/c_src/jack_midi.c
   TODO: Add groove, sample sync. */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"
#include "assert_write.h"
#include "uct_byteswap.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#include "jack_tools.h"

#include "tag_u32.h"


/* QUEUES */

/* Erlang input thread to Jack thread buffer. */
#define NB_FROM_ERL_BUFS 8
#define MIDI_CMD_SIZE (4 + 64)
struct command {
    uint32_t size;  // {packet,4}
    uint8_t  midi_bytes[MIDI_CMD_SIZE];
} __attribute__((__packed__));
static struct command from_erl_buf[NB_FROM_ERL_BUFS];
static volatile unsigned int from_erl_read = 0, from_erl_write = 0; // FIXME: atomic?
static inline uint32_t command_nb_midi_bytes(struct command *c) {
    return c->size;
}


/* JACK */
static jack_port_t *audio_out = NULL;
static jack_port_t *midi_out = NULL;
static jack_port_t *midi_in = NULL;
static jack_client_t *client = NULL;
int nb_midi_drop = 0;
jack_nframes_t clock_hperiod = 0;

// Send midi data out over a jack port.
static inline void send_midi(void *out_buf, jack_nframes_t time,
                             const void *data_buf, size_t nb_bytes) {
    //LOG("%d %d %d\n", frames, time, (int)nb_bytes);
    void *buf = jack_midi_event_reserve(out_buf, time, nb_bytes);
    if (buf) {
        memcpy(buf, data_buf, nb_bytes);
    }
    else {
        // Nothing to do but drop.
        nb_midi_drop++;
    }
}

#define BPM_TO_HPERIOD(sr,bpm) ((sr*5)/(bpm*4))

static jack_nframes_t bpm = 120;
static int clock_phase = 0.0;
static int clock_pol = 1;

static int process (jack_nframes_t nframes, void *arg) {

    /* Boilerplate. */
    jack_nframes_t sr = jack_get_sample_rate(client);
    jack_default_audio_sample_t *audio_out_buf = jack_port_get_buffer(audio_out, nframes);
    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
    jack_midi_clear_buffer(midi_out_buf);

    /* Jack requires us to sort the events, so send the async data
       first using time stamp 0.  This consists of data received from
       Erlang, and MIDI data on in. */
    while(from_erl_read != from_erl_write) {
        struct command *cmd = &from_erl_buf[from_erl_read];
        uint32_t nb_midi = command_nb_midi_bytes(cmd);
        uint8_t *midi = &cmd->midi_bytes[0];
        send_midi(midi_out_buf, 0/*time*/, midi, nb_midi);
        from_erl_read = (from_erl_read + 1) % NB_FROM_ERL_BUFS;
    }
    FOR_MIDI_EVENTS(iter, midi_in, nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 1) {
            switch(msg[0]) {
                /* Filter out start, continue, stop */
            case 0xFA:
            case 0xFB:
            case 0xFC:
                send_midi(midi_out_buf, 0/*time*/, msg, 1);
                break;
            }
        }
    }

    /* This is an integer divisor of the sample clock, which makes it
       possible to have perfect lock for devices that only receive
       word clock in. */
    if (!clock_hperiod) {
        /* It seems that sr is only valid in side the process thread,
           so set it here once. */
        clock_hperiod = BPM_TO_HPERIOD(sr, bpm);
        float bpm_actual = (((float)sr)*1.25f) / ((float)clock_hperiod);
        LOG("clock: bpm_set = %d, sr = %d -> clock_hperiod = %d, bpm_actual = %3.6f\n", bpm, sr, clock_hperiod, bpm_actual);
        // FIXME: Send out an NRPN as well?
    }

    /* Generate quare wave output, send midi on positive edge. */
    for (int t=0; t<nframes; t++) {
        if (clock_phase >= clock_hperiod) {
            clock_phase -= clock_hperiod;
            clock_pol ^= 1;
            if (clock_pol == 1) {
                const uint8_t clock[] = {0xF8};
                send_midi(midi_out_buf, t, clock, sizeof(clock));
            }
        }
        audio_out_buf[t] = clock_pol;
        clock_phase += 1;
    }

    return 0;
}





static void handle_midi(const uint8_t *midi, uint32_t nb) {

    while (((from_erl_write - from_erl_read) % NB_FROM_ERL_BUFS)
           == (NB_FROM_ERL_BUFS - 1)) {
        /* Buffer is full.  This is unlikely: buffer size should be
           chosen so this doesn't happen.  Since we manage the buffer
           (and not the OS), what we can do here is to sleep to ensure
           one buffer tick will happen.  This could use a condition
           variable.  It doesn't seem so important.  Just don't flood
           the buffer. */
        LOG("input stall\n");
        usleep(1000);
    }

    struct command *cmd = &from_erl_buf[from_erl_write];
    cmd->size = nb;
    memcpy(cmd->midi_bytes, midi, nb);
    // LOG("msg: n=%d\n", cmd->size);
    from_erl_write = (from_erl_write + 1) % NB_FROM_ERL_BUFS;
}


void send_tag_u32_buf_write(const uint8_t *buf, uint32_t len) {
    uint8_t len_buf[4];
    write_be(len_buf, len, 4);
    assert_write(1, len_buf, 4);
    assert_write(1, buf, len);
}
#define SEND_TAG_U32_BUF_WRITE send_tag_u32_buf_write
#include "mod_send_tag_u32.c"

const char t_map[] = "map";
const char t_cmd[] = "cmd";

int reply_1(struct tag_u32 *req, uint32_t rv) {
    SEND_REPLY_TAG_U32(req, rv);
    return 0;
}
int reply_2(struct tag_u32 *req, uint32_t rv1, uint32_t rv2) {
    SEND_REPLY_TAG_U32(req, rv1, rv2);
    return 0;
}
int reply_ok(struct tag_u32 *req) {
    return reply_1(req, 0);
}
int reply_ok_1(struct tag_u32 *req, uint32_t val) {
    return reply_2(req, 0, val);
}
int reply_error(struct tag_u32 *req) {
    return reply_1(req, -1);
}
int handle_clock_div(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, div) {
        LOG("set sample clock div = %d\n", m->div);
        clock_hperiod = m->div / 2; // FIXME: rounding!
        return reply_ok(req);
    }
    return -1;
}
int map_root(struct tag_u32 *req) {
    const struct tag_u32_entry map[] = {
        {"clock_div", t_cmd, handle_clock_div, 1},
    };
    return HANDLE_TAG_U32_MAP(req, map);
}
int handle_tag_u32(struct tag_u32 *req) {
    int rv = map_root(req);
    if (rv) {
        LOG("handle_tag_u32 returned %d\n", rv);
        /* Always send a reply when there is a from address. */
        send_reply_tag_u32_status_cstring(req, 1, "bad_ref");
    }
    return 0;
}

int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "clock"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    ASSERT(midi_in = jack_port_register(
               client, "control",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));

    ASSERT(midi_out = jack_port_register(
               client, "midi",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0));

    ASSERT(audio_out = jack_port_register(
               client, "wave",
               JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* The main thread blocks on stdin.  The protocol is {packet,4}
       with MIDI on TAG_STREAM. */
    for(;;) {
        uint32_t nb; {
            uint8_t buf[4];
            assert_read_fixed(0, buf, 4);
            nb = read_be(buf, 4);
        }
        LOG("nb = %d\n", nb);
        uint8_t buf[nb];
        assert_read_fixed(0, buf, nb);
        if (nb >= 4) {
            uint16_t tag = read_be(buf, 2);
            LOG("tag = %04x\n", tag);
            switch(tag) {
            case 0xFFFB: {
                uint16_t stream = read_be(buf + 2, 2);
                LOG("stream = %04x\n", stream);
                if (stream == 0) {
                    handle_midi(buf + 4, nb - 4);
                }
                break;
            }
            case TAG_U32:
                tag_u32_dispatch(handle_tag_u32,
                                 send_reply_tag_u32,
                                 NULL,
                                 buf, nb);
                break;

            }
        }
    }
    return 0;
}

