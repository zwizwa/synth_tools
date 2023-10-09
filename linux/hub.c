/* Erlang MIDI hub.

   Handles all midi/Erlang routing.
   Hosts sequencer / arpeggiator.

   Clock is always slave mode here to keep things flexible.
   In my setup, clock.c is master clock.

   Note that this has all equipment hardcoded.  I currently do not see
   the point in adding a layer of configuration abstraction.  Easy
   enough to recompile in the current setup, so all config is in C, or
   C generated from compile-time config.  Later it might become
   obvious how to separate this out into config and generic code.

*/

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "jack_tools.h"
#include "erl_port.h"
#include "assert_read.h"
#include "assert_write.h"



/* JACK */
#define FOR_MIDI_IN(m) \
    m(clock_in)     \
    m(fire_in)      \
    m(easycontrol)  \
    m(keystation_in1)  \
    m(keystation_in2)  \
    m(z_debug)     \

#define FOR_MIDI_IN_DIS(m) \

#define FOR_MIDI_OUT(m) \
    m(tb03)         \
    m(fire_out)     \
    m(volca_keys)   \
    m(volca_bass)   \
    m(volca_beats)  \
    m(synth)        \
    m(pd_out)       \


FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

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
#define TO_ERL_SIZE_LOG 12
#define TO_ERL_SIZE (1 << TO_ERL_SIZE_LOG)
static uint8_t to_erl_buf[TO_ERL_SIZE];
static size_t to_erl_buf_bytes = 0;
//static uint32_t to_erl_room(void) {
//    uint32_t free_bytes = sizeof(to_erl_buf) - to_erl_buf_bytes;
//    if (free_bytes >= 6) return free_bytes - 6;
//    return 0;
//}
static uint8_t *to_erl_hole(int nb, uint16_t port) {
    size_t msg_size = 6 + nb;
    if (to_erl_buf_bytes + msg_size > sizeof(to_erl_buf)) {
        LOG("midi buffer overflow\n");
        return NULL;
    }
    uint8_t *msg = &to_erl_buf[to_erl_buf_bytes];
    set_u32be(msg, msg_size - 4); // {packet,4}
    msg[4] = (port >> 8);
    msg[5] = port;
    to_erl_buf_bytes += msg_size;
    return &msg[6];
}
static void to_erl(const uint8_t *buf, int nb, uint8_t port) {
    uint8_t *hole = to_erl_hole(nb, port);
    if (hole) { memcpy(hole, buf, nb); }
}


static uint32_t phase = 0;
static uint32_t running = 0;


static inline void process_z_debug(jack_nframes_t nframes) {
    FOR_MIDI_EVENTS(iter, z_debug, nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        LOG_HEX("z_debug:", msg, n);
    }
}


static inline void process_clock_in(jack_nframes_t nframes) {
    void *pd_out_buf = midi_out_buf(pd_out, nframes);

    FOR_MIDI_EVENTS(iter, clock_in, nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 1) {
            switch(msg[0]) {
            case 0xFA: // start
                running = 1;
                phase = 0;
                break;
            case 0xFB: // continue
                running = 1;
                break;
            case 0xFC: // stop
                running = 0;
                break;
            case 0xF8: { // clock
                if (running) {
                    uint32_t qn = phase == 0; // one quarter note
                    uint32_t en = (phase % 12) == 0; // one eight note

                    /* For Pd Note On/Off is not used as it requires begin/end of events.
                       Use CC to map better to single-ended events. */
                    if (qn) {
                        send_cc(pd_out_buf, 0, 0, 0);
                    }
                    if (en) {
                        send_cc(pd_out_buf, 0, 0, 1);
                    }
                }
                phase = (phase + 1) % 24;
                break;
            }
            }
        }
    }
}

// FIXME: I want a simpler midi dispatch construct.

static inline void process_easycontrol_in(jack_nframes_t nframes, uint8_t stamp) {
    FOR_MIDI_EVENTS(iter, easycontrol, nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl(msg, n, 0 /*midi port*/);
        if (n == 3) {
            switch(msg[0]) {
            case 0xb0: {
                uint8_t cc  = msg[1];
                uint8_t val = msg[2];
                switch(cc) {
                    case 0x2d:
                        if (!val) {
                            /* Play press. */
                            LOG("easycontrol: start\n");
                        }
                        break;
                    case 0x2e:
                        if (!val) {
                            /* Stop press. */
                            LOG("easycontrol: stop\n");
                        }
                        break;
                }
                break;
            }
            }
        }
    }
}

static inline void process_keystation_in1(jack_nframes_t nframes, uint8_t stamp) {
    FOR_MIDI_EVENTS(iter, keystation_in1, nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl(msg, n, 0 /*midi port*/);
    }
}
static inline void process_keystation_in2(jack_nframes_t nframes, uint8_t stamp) {
    FOR_MIDI_EVENTS(iter, keystation_in2, nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl(msg, n, 0 /*midi port*/);
        if (n == 3) {
            switch(msg[0]) {
            case 0x90: { /* Note on */
                uint8_t note = msg[1];
                // uint8_t vel  = msg[2];
                switch(note) {
                    case 0x5e:
                        /* Play press. */
                        LOG("keystation: start\n");
                        break;
                    case 0x5d:
                        /* Stop press. */
                        LOG("keystation: stop\n");
                        break;
                }
                break;
            }
            }
        }
    }
}




static inline void process_erl_out(jack_nframes_t nframes) {
    /* Send to Erlang

       Note: I'm not exactly sure whether it is a good idea to perform
       the write() call from this thread, but it seems the difference
       between a single semaphore system call and a single write to an
       Erlang port pipe accessing a single page of memory is not going
       to be big.  So revisit if it ever becomes a problem.

       This will buffer all midi messages and perform only a single
       write() call.

    */

    if (to_erl_buf_bytes) {
        //LOG("buf_bytes = %d\n", (int)to_erl_buf_bytes);
        assert_write(1, to_erl_buf, to_erl_buf_bytes);
        to_erl_buf_bytes = 0;
    }

}

static int process (jack_nframes_t nframes, void *arg) {

    /* Erlang out is tagged with a rolling time stamp. */
    jack_nframes_t f = jack_last_frame_time(client);
    uint8_t stamp = (f / nframes);

    /* Order is important. */
    process_z_debug(nframes);
    process_clock_in(nframes);
    process_easycontrol_in(nframes, stamp);
    process_keystation_in1(nframes, stamp);
    process_keystation_in2(nframes, stamp);
    process_erl_out(nframes);

    return 0;
}



int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "hub"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);
    FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
    FOR_MIDI_OUT(REGISTER_JACK_MIDI_OUT);

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* The main thread blocks on stdin.  The protocol is lowest common
       denominator.  While these are written to interface to Erlang,
       let's use midi as the main protocol so it is easier to reuse in
       different configurations. */
    for(;;) {
        // FIXME: Current function is just to exit when stdin is closed.
        uint8_t buf[1];
        assert_read(0, buf, sizeof(buf));
        exit(1);
    }
    return 0;
}

