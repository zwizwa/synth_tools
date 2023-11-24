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
#include "tag_u32.h"

#include "mod_sequencer.c"

void send_tag_u32_buf_write(const uint8_t *buf, uint32_t len) {
    uint8_t len_buf[4];
    write_be(len_buf, len, 4);
    assert_write(1, len_buf, 4);
    assert_write(1, buf, len);
}
#define SEND_TAG_U32_BUF_WRITE send_tag_u32_buf_write
#include "mod_send_tag_u32.c"

/* JACK */
#define FOR_MIDI_IN(m) \
    m(clock_in)        \
    m(fire_in)         \
    m(easycontrol)     \
    m(keystation_in1)  \
    m(keystation_in2)  \
    m(remote_in)       \
    m(uma_in)          \
    m(z_debug)         \

#define FOR_MIDI_IN_DIS(m) \

#define FOR_MIDI_OUT(m) \
    m(tb03)         \
    m(fire_out)     \
    m(volca_keys)   \
    m(volca_bass)   \
    m(volca_beats)  \
    m(synth)        \
    m(pd_out)       \
    m(transport)    \


FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

struct remote {
    uint8_t sel;
    uint8_t record;
};

struct app {
    struct sequencer sequencer;
    uint32_t running;
    jack_nframes_t nframes;
    uint8_t stamp;

    /* stateful processors */
    struct remote remote;

    /* midi out ports */
    void *pd_out_buf;
    void *transport_buf;

    /* rolling time */
    uint32_t time;

} app_state = {};

#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

static inline void *midi_out_buf_cleared(jack_port_t *port, jack_nframes_t nframes) {
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
static inline void send_control_byte(void *out_buf, uint8_t byte) {
    send_midi(out_buf, 0, &byte, 1);
}
static inline void send_start(void *out_buf) { send_control_byte(out_buf, 0xFA); }
static inline void send_stop(void *out_buf)  { send_control_byte(out_buf, 0xFC); }


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
static uint8_t *to_erl_hole_8(int nb, uint16_t port) {
    size_t msg_size = 8 + nb;
    if (to_erl_buf_bytes + msg_size > sizeof(to_erl_buf)) {
        LOG("erl buffer overflow\n");
        return NULL;
    }
    uint8_t *msg = &to_erl_buf[to_erl_buf_bytes];
    /* Midi is mapped into generic stream tag.  Maybe this should have
       its own tag?  We do need to guarantee single midi messages
       here. */
    set_u32be(msg, msg_size - 4); // {packet,4}
    set_u16be(msg+4, 0xFFFB); // TAG_STREAM
    set_u16be(msg+6, port);
    to_erl_buf_bytes += msg_size;
    return &msg[8];
}
static uint8_t *to_erl_hole_6(int nb) {
    size_t msg_size = 6 + nb;
    if (to_erl_buf_bytes + msg_size > sizeof(to_erl_buf)) {
        LOG("erl buffer overflow\n");
        return NULL;
    }
    uint8_t *msg = &to_erl_buf[to_erl_buf_bytes];
    set_u32be(msg, msg_size - 4); // {packet,4}
    set_u16be(msg+4, 0xFFEE); // TAG_PTERM
    to_erl_buf_bytes += msg_size;
    return &msg[6];
}
static void to_erl_pterm(const char *pterm) {
    int nb = strlen(pterm);
    uint8_t *hole = to_erl_hole_6(nb);
    if (hole) { memcpy(hole, pterm, nb); }
}
static void to_erl_ptermvf(const char *fmt, va_list ap) {
    char *pterm = NULL;
    ASSERT(-1 != vasprintf(&pterm, fmt, ap));
    to_erl_pterm(pterm);
}
// FIXME: This is so common it deserves a macro in uc_tools
static void to_erl_ptermf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    to_erl_ptermvf(fmt, ap);
    va_end(ap);
}

static void to_erl_midi(const uint8_t *buf, int nb, uint8_t port) {
    uint8_t *hole = to_erl_hole_8(nb, port);
    if (hole) { memcpy(hole, buf, nb); }
}

static inline void process_z_debug(struct app *app) {
    FOR_MIDI_EVENTS(iter, z_debug, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        LOG_HEX("z_debug:", msg, n);
    }
}

void pat_tick(struct sequencer *seq, const struct pattern_step *step) {
    struct app *app = (void*)seq;
    const uint8_t *msg = step->event.as.u8;
    LOG("pat_tick %02x %02x %02x %02x\n", msg[0], msg[1], msg[2], msg[3]);

    if (msg[0] < 16) {
        // FIXME: msg[0] is midi port, make numerical mapping
        send_midi(app->pd_out_buf, 0, msg + 1, 3);
    }
    else {
        LOG("unsupported event tag %d\n", msg[0]);
    }
}
void pattern_init(struct sequencer *s) {
    sequencer_init(s, pat_tick);
    sequencer_restart(s);
}

static inline void app_play(struct app *app) {
    LOG("app_play\n");
    app->running = 1;
}
static inline void app_continue(struct app *app) {
    LOG("app_continue\n");
    app->running = 1;
}
static inline void app_pause(struct app *app) {
    LOG("app_pause\n");
    app->running = 0;
}
static inline void app_stop(struct app *app) {
    LOG("app_stop\n");
    app->running = 0;
    sequencer_restart(&app->sequencer);
}


static inline void process_clock_in(struct app *app) {
    FOR_MIDI_EVENTS(iter, clock_in, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 1) {
            switch(msg[0]) {
            case 0xFA: // start
                app_play(app);
                break;
            case 0xFB: // continue
                app_continue(app);
                break;
            case 0xFC: // stop
                app_stop(app);
                break;
            case 0xF8: { // clock
                // LOG("tick, running=%d\n", app->running);
                if (app->running) {
                    sequencer_tick(&app->sequencer);
                }
                break;
            }
            }
        }
    }
}

// FIXME: I want a simpler midi dispatch construct.

static inline void process_easycontrol_in(struct app *app) {
    FOR_MIDI_EVENTS(iter, easycontrol, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl_midi(msg, n, 0 /*midi port*/);
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
                            send_start(app->transport_buf);
                        }
                        break;
                    case 0x2e:
                        if (!val) {
                            /* Stop press. */
                            LOG("easycontrol: stop\n");
                            send_stop(app->transport_buf);
                        }
                        break;
                }
                break;
            }
            }
        }
    }
}

static inline void process_keystation_in1(struct app *app) {
    FOR_MIDI_EVENTS(iter, keystation_in1, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl_midi(msg, n, 1 /*midi port*/);
    }
}
static inline void process_keystation_in2(struct app *app) {
    FOR_MIDI_EVENTS(iter, keystation_in2, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl_midi(msg, n, 2 /*midi port*/);
        if (n == 3) {
            switch(msg[0]) {
            case 0x90: { /* Note on */
                uint8_t note = msg[1];
                // uint8_t vel  = msg[2];
                switch(note) {
                    case 0x5e:
                        /* Play press. */
                        LOG("keystation: start\n");
                        send_start(app->transport_buf);
                        app_play(app);
                        break;
                    case 0x5d:
                        /* Stop press. */
                        LOG("keystation: stop\n");
                        send_stop(app->transport_buf);
                        app_stop(app);
                        break;
                }
                break;
            }
            }
        }
    }
}

void pd_midi(struct app *app, const uint8_t *msg, size_t len) {
    /* Jack midi port connected to pd_io object, which takes jack midi
     * in and converts it to netsend into Pd. */
    send_midi(app->pd_out_buf, 0, msg, len);
    /* Send a copy to Erlang. */
    to_erl_midi(msg, len, 4 /* midi port */);
}
void pd_cc(struct app *app, uint8_t ctrl, uint8_t val) {
    // Map it back to a CC after stateful processing
    uint8_t msg[] = {
        0xB0 + (app->remote.sel & 0x0F),
        ctrl & 0x7F,
        val & 0x7F
    };
    pd_midi(app, msg, sizeof(msg));
}
void pd_note(struct app *app, uint8_t on_off, uint8_t note, uint8_t vel) {
    // Route it to the proper channel
    uint8_t msg[] = {
        (on_off & 0xF0) + (app->remote.sel & 0xF),
        note & 0x7f,
        vel & 0x7f
    };
    pd_midi(app, msg, sizeof(msg));

    // Recording
    if (app->remote.record) {
        /* The recorder is implemented in Erlang.

           Since traffic is one-way only, let's use a protocol that is
           convenient to parse at the Erlang side and easy to generate
           here: printed terms.  Is also easy to embed in sysex as
           ASCII. */

        to_erl_ptermf(
            "{record,{%d,{%d,%d,%d,%d}}}",
            app->time,
            PAT_MIDI_TAG(0), // FIXME: ports
            msg[0], msg[1], msg[2]);
    }

}

static inline void process_remote_in(struct app *app) {
    struct remote *r = &app->remote;
    FOR_MIDI_EVENTS(iter, remote_in, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        uint8_t tag = msg[0];
        if (n == 3) {
            switch(tag) {
            case 0x80:
            case 0x90: {
                /* Route it to the current track. */
                uint8_t note = msg[1];
                uint8_t vel = msg[2];
                pd_note(app, tag, note, vel);
                break;
            }
            case 0xB0: {
                /* Template 64 Zwizwa Exo has all knobs, sliders,
                   encoders mapped to CC in a linear fashion. */
                uint8_t cc = msg[1];
                uint8_t val = msg[2];
                if (cc <= 7) {
                    uint8_t slider = cc;
                    r->sel = slider;
                    pd_cc(app, 0, val);
                }
                else if (cc <= 15) {
                    uint8_t slider_but = cc - 8;
                    r->sel = slider_but;
                    pd_cc(app, 1, val);
                }
                else if (cc <= 23) {
                    uint8_t knob = cc - 16;
                    r->sel = knob;
                    pd_cc(app, 2, val);
                }
                else if (cc <= 31) {
                    uint8_t knob_but = cc - 24;
                    r->sel = knob_but;
                    pd_cc(app, 3, val);
                }
                else if (cc <= 39) {
                    uint8_t rotary = cc - 32;
                    // local to r->sel
                    // FIXME: do rotary processing
                    pd_cc(app, 4 + rotary, val);
                }
                else if (cc <= 47) {
                    uint8_t rotary_but = cc - 40;
                    // local to r->sel
                    pd_cc(app, 4 + 8 + rotary_but, val);
                }
                else if (cc == 0x32) {
                    // stop
                    if (app->remote.record) {
                        to_erl_pterm("{record,stop}");
                        app->remote.record = 0;
                    }
                    else {
                        send_stop(app->transport_buf);
                    }
                }
                else if (cc == 0x33) {
                    // play
                    if (app->remote.record) {
                        to_erl_pterm("{record,play}}");
                    }
                    else {
                        send_start(app->transport_buf);
                    }
                }
                else if (cc == 0x34) {
                    // rec
                    /* This is tricky.  What we really want to do is
                       to track the state of the record LED, which
                       toggles when the button is pressed, and turns
                       off when stop is pressed.  Assume that the
                       initial state is off.  It's not sending the LED
                       state. */
                    to_erl_midi(msg, n, 3 /*midi port*/);
                    if (val == 0) {
                        app->remote.record = !app->remote.record;
                        if (app->remote.record) {
                            app->time = 0;
                            to_erl_pterm("{record,start}");
                        }
                        else {
                            to_erl_pterm("{record,stop}");
                        }
                    }
                    else {
                        /* Button is configured as momentary to allow
                           for later use of the release event. */
                    }
                }
                else {
                    to_erl_midi(msg, n, 3 /*midi port*/);
                }
                break;
            }
            default: {
                to_erl_midi(msg, n, 3 /*midi port*/);
                break;
            }
            }
        }
        else {
            to_erl_midi(msg, n, 3 /*midi port*/);
        }
    }
}

static inline void process_uma_in(struct app *app) {
    // Just reuse the remote25 struct. Never used together.
    struct remote *r = &app->remote;
    FOR_MIDI_EVENTS(iter, uma_in, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */

        uint8_t tag = msg[0];
        if (n == 3) {
            switch(tag) {
            case 0x80:
            case 0x90: {
                /* Route it to the current track. */
                uint8_t note = msg[1];
                uint8_t vel = msg[2];
                LOG("note %d %d\n", note, vel);
                pd_note(app, tag, note, vel);
                break;
            }
            case 0xB0: {
                /* Template 64 Zwizwa Exo has all knobs, sliders,
                   encoders mapped to CC in a linear fashion. */
                uint8_t cc = msg[1];
                uint8_t val = msg[2];
                if (cc == 25) {
                    app_stop(app);
                }
                else if (cc == 26) {
                    app_pause(app);
                }
                else if (cc == 27) {
                    app_play(app);
                }
                else if (cc == 28) {
                    if (val == 0) {
                        LOG("rec on\n");
                        to_erl_pterm("{record,start}");
                        r->record = 1;
                    }
                    else {
                        LOG("rec off\n");
                        to_erl_pterm("{record,stop}");
                        r->record = 0;
                    }

                    // app_play(app);


                }
                LOG("CC %d %d\n", cc, val);
            }
            default:
                to_erl_midi(msg, n, 6 /*midi port*/);
                break;
            }
        }
        else {
            to_erl_midi(msg, n, 6 /*midi port*/);
        }
    }
}




static inline void process_erl_out(struct app *app) {
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

static void app_process(struct app *app) {

    /* Erlang out is tagged with a rolling time stamp. */
    jack_nframes_t f = jack_last_frame_time(client);
    app->stamp = (f / app->nframes);

    /* Order is important. */
    process_z_debug(app);
    process_clock_in(app);
    process_easycontrol_in(app);
    process_keystation_in1(app);
    process_keystation_in2(app);
    process_remote_in(app);
    process_uma_in(app);
    process_erl_out(app);
}

static int process (jack_nframes_t nframes, void *arg) {
    struct app *app = &app_state;
    app->nframes = nframes;
    app->pd_out_buf    = midi_out_buf_cleared(pd_out, nframes);
    app->transport_buf = midi_out_buf_cleared(transport, nframes);
    app_process(app);
    app->time += nframes;
    return 0;
}

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

/* Note that the hub no longer contains the master clock, so for now
   we need to ignore this.  How to fix?  Erlang has direct access to
   the clock object so maybe best it is sent there. */
int handle_clock_div(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, div) {
        LOG("FIXME: set sample clock div = %d\n", m->div);
        return reply_ok(req);
    }
    return -1;
}

int handle_pattern_begin(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, nb_clocks) {
        struct app *app = req->context;
        struct sequencer *s = &app->sequencer;
        if (s->transaction.pattern != PATTERN_NONE) {
            LOG("already current pattern\n");
            return reply_error(req);
        }
        if (m->nb_clocks == 0) {
            LOG("can't have nb_clocks == 0\n");
            return reply_error(req);
        }
        pattern_t pat_nb = sequencer_pattern_begin(s, m->nb_clocks);
        return reply_ok_1(req, pat_nb);
    }
    return -1;
}

int handle_pattern_end(struct tag_u32 *req) {
    struct app *app = req->context;
    struct sequencer *s = &app->sequencer;
    if (s->transaction.pattern == PATTERN_NONE) {
        LOG("no current pattern\n");
        return reply_error(req);
    }
    sequencer_pattern_end(s);
    return reply_ok(req);
}

// FIXME: Use a byte interface for the events.
int handle_step(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, type, arg0, arg1, arg2, delay) {
        LOG("add to pattern event %d,%d,%d,%d delay %d\n",
            m->type, m->arg0, m->arg1, m->arg2, m->delay);
        struct app *app = req->context;
        struct sequencer *s = &app->sequencer;
        if (s->transaction.pattern == PATTERN_NONE) {
            LOG("no current pattern\n");
            return reply_error(req);
        }
        struct pattern_event ev = { .as = {
                .u8[0] = m->type,
                .u8[1] = m->arg0,
                .u8[2] = m->arg1,
                .u8[3] = m->arg2,
            }};
        sequencer_pattern_step(s, &ev, m->delay);
        return reply_ok(req);
    }
    return -1;
}
int map_root(struct tag_u32 *req) {
    const struct tag_u32_entry map[] = {
        {"clock_div",     t_cmd, handle_clock_div, 1},
        {"pattern_begin", t_cmd, handle_pattern_begin, 0},
        {"pattern_end",   t_cmd, handle_pattern_end, 0},
        {"step",          t_cmd, handle_step, 5},
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

    struct app *app = &app_state;
    pattern_init(&app->sequencer);

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

    /* Use the generic {packet,4} + tag protocol on stdin, since hub.c
       might be hosting a lot of in-image functionality later. */
    for(;;) {
        uint8_t size_be[4];
        assert_read(0, size_be, 4);
        uint32_t size = read_be(size_be, 4);
        uint8_t buf[size];
        assert_read(0, buf, size);
        ASSERT(size >= 2);
        uint16_t tag = read_be(buf, 2);
        switch(tag) {
        case TAG_U32: {
            tag_u32_dispatch(handle_tag_u32,
                             send_reply_tag_u32,
                             app,
                             buf, size);
            break;
        }
        default:
            ERROR("unknown tag 0x%04x\n", tag);
        }
    }
    return 0;
}

