/* MIDI Clock code lifted from studio/c_src/jack_midi.c
   TODO: Add groove, sample sync. */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"
#include "uct_byteswap.h"

#include <jack/jack.h>
#include <jack/midiport.h>

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

#define BPM_TO_HPERIOD(sr,bpm) ((sr*30)/(bpm*24))
static jack_nframes_t bpm = 120;
static float clock_phase = 0.0f;
static int clock_pol = 1;

static int process (jack_nframes_t nframes, void *arg) {

    /* Boilerplate. */
    jack_nframes_t sr = jack_get_sample_rate(client);
    jack_default_audio_sample_t *audio_out_buf = jack_port_get_buffer(audio_out, nframes);
    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
    jack_midi_clear_buffer(midi_out_buf);

    /* Jack requires us to sort the events, so send the async data
       first using time stamp 0. */
    while(from_erl_read != from_erl_write) {
        struct command *cmd = &from_erl_buf[from_erl_read];
        uint32_t nb_midi = command_nb_midi_bytes(cmd);
        uint8_t *midi = &cmd->midi_bytes[0];
        send_midi(midi_out_buf, 0/*time*/, midi, nb_midi);
        from_erl_read = (from_erl_read + 1) % NB_FROM_ERL_BUFS;
    }

    /* FIXME: It's probably best to make it so that drum clock can be
       derived from sample clock using an integer ratio.  That way
       only wordclock needs to be distributed to isolated hardware. */
    float clock_hperiod = BPM_TO_HPERIOD(sr, bpm);

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
            }
        }
    }
    return 0;
}

