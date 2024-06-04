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

/* Some general notes about structure

   There are two kinds of "state" here:

   - External state that is manipulated by sending messages,
     i.e. synths and drum machines audio algorithm state, controller
     UI state.

   - Internal state that needs to be _queried_ to perform certain
     actions.  E.g. sequencer, mmc state.

   It feels cleaner if the state is read inside the private methods
   only, but this is not always possible, and is an indication that
   structure is not ideal, i.e. tight interaction is needed between
   objects.



*/


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "jack_tools.h"
#include "assert_read.h"
#include "tag_u32.h"

#include "mod_sequencer.c"
#include "mod_akai_fire.c"
#include "mod_novation_remote.c"
#include "mod_arturia_minilab.c"
#include "mod_maudio_axiom25.c"

#include "mod_to_erl.c"

#define TELNET_WORD_MODE
#include "mod_telnet.c"



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
    m(akai_fire_in)    \
    m(maudio_axiom25_in)        \
    m(easycontrol)     \
    m(arturia_minilab_in)      \
    m(keystation_in1)  \
    m(keystation_in2)  \
    m(novation_remote_in) \
    m(uma_in)          \
    m(z_debug)         \

#define FOR_MIDI_IN_DIS(m) \

#define FOR_MIDI_OUT(m) \
    m(tb03)         \
    m(fire_out)     \
    m(volca_keys)   \
    m(volca_bass)   \
    m(volca_beats)  \
    m(synth_out)    \
    m(pd_out)       \
    m(transport)    \


FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

struct route { };
struct pd { };
struct synth { };
struct mmc {
    uint32_t time;      /* rolling time */
    uint32_t running:1;
    uint32_t record:1;
};

// figure out how to map struct to parent


struct app {
    struct sequencer sequencer;
    jack_nframes_t nframes;
    uint8_t stamp;

    /* message handler state */
    struct route route;
    struct novation_remote novation_remote;
    struct arturia_minilab arturia_minilab;
    struct maudio_axiom25 maudio_axiom25;
    struct akai_fire akai_fire;
    struct pd pd;
    struct mmc mmc;
    struct synth synth;

    /* midi out ports */
    void *pd_out_buf;
    void *transport_buf;
    void *fire_out_buf;
    void *synth_out_buf;

    /* telnet control port */
    struct telnet telnet;
    int telnet_fd;
    jack_ringbuffer_t *telnet_ringbuffer;
    //void (*telnet_op)(struct app *app);
    //uintptr_t telnet_lit;

} app_state = {};



/* Cross-link */
#include "uct_offsetof.h"

#define DEF_TO_APP(substruct)                   \
    DEF_FIELD_TO_PARENT(                        \
        substruct##_to_app,                     \
        struct app,                             \
        struct substruct,                       \
        substruct)                              \

DEF_TO_APP(sequencer)
DEF_TO_APP(akai_fire)
DEF_TO_APP(mmc)
DEF_TO_APP(pd)
DEF_TO_APP(route)
DEF_TO_APP(telnet)




#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

static inline void *midi_out_buf_cleared(jack_port_t *port, jack_nframes_t nframes) {
    void *buf = jack_port_get_buffer(port, nframes);
    jack_midi_clear_buffer(buf);
    return buf;
}



static inline void process_z_debug(struct app *app) {
    FOR_MIDI_EVENTS(iter, z_debug, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        LOG_HEX("z_debug:", msg, n);
    }
}


void mmc_play(struct mmc *mmc) {
    LOG("mmc_play %d->1\n", mmc->running);
    mmc->running = 1;
    struct app *app = mmc_to_app(mmc);
    send_start(app->transport_buf);
}
void mmc_continue(struct mmc *mmc) {
    LOG("mmc_continue\n");
    mmc->running = 1;
    struct app *app = mmc_to_app(mmc);
    send_continue(app->transport_buf);
}
void mmc_stop(struct mmc *mmc) {
    LOG("mmc_stop %d->0\n", mmc->running);
    mmc->running = 0;
    struct app *app = mmc_to_app(mmc);
    send_stop(app->transport_buf);
    sequencer_restart(&app->sequencer);
}
void mmc_toggle(struct mmc *mmc) {
    if (mmc->running) {
        mmc_stop(mmc);
    }
    else {
        mmc_play(mmc);
    }
}
int mmc_record(struct mmc *mmc) {
    return mmc->record;
}
void mmc_set_record(struct mmc *mmc, int record) {
    mmc->record = !!record;
}
void mmc_toggle_record(struct mmc *mmc) {
    mmc->record = !mmc->record;
}

void mmc_reset_time(struct mmc *mmc) {
    mmc->time = 0;
}
int mmc_running(struct mmc *mmc) {
    return mmc->running;
}


static inline void process_clock_in(struct app *app) {
    struct mmc *mmc = &app->mmc;
    FOR_MIDI_EVENTS(iter, clock_in, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 1) {
            switch(msg[0]) {
            case 0xFA: // start
                LOG("clock_in start->app_play\n");
                mmc_play(mmc);
                break;
            case 0xFB: // continue
                mmc_continue(mmc);
                break;
            case 0xFC: // stop
                mmc_stop(mmc);
                break;
            case 0xF8: { // clock
                // LOG("tick, running=%d\n", app->running);
                if (mmc->running) {
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
    struct mmc *mmc = &app->mmc;
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
                        mmc_play(mmc);
                        break;
                    case 0x5d:
                        /* Stop press. */
                        LOG("keystation: stop\n");
                        mmc_stop(mmc);
                        break;
                }
                break;
            }
            }
        }
    }
}

/* Data flow:  FIXME TODO

   - The midi drivers think in terms of midi messages and 'selectors',
     which are like channels but have a larger span.

   - We convert that data to pattern_event

   - It is the pattern_event data that is routed

*/


/* Map selector to port/channel. */
void route_pattern_event(struct route *route, const union pattern_event *ev) {

    const uint8_t *msg = &ev->u8[1];
    int len = 3; // FIXME: Depends on the contents of the event.  Currently only note, cc.

    struct app *app = route_to_app(route);
    int port = ev->u8[0] & 0x0F; // FIXME: Assumes midi
    switch(port) {
    /* Jack midi port connected to pd_io object, which takes jack
       midi in and converts it to netsend into Pd. */
    case 0: send_midi(app->pd_out_buf, 0, msg, len); break;
    /* synth.c */
    case 1: send_midi(app->synth_out_buf, 0, msg, len); break;
    }

    /* Send a copy to Erlang. */
    to_erl_midi(msg, len, port);
}
void app_sequencer_tick(struct sequencer *seq, const union pattern_event *ev) {
    struct app *app = (void*)seq;
    route_pattern_event(&app->route, ev);
}



/* These are used by device drivers to send midi to a specific device.
   The 'sel' here is not a midi port or channel, but a unique id that
   is later mapped to port/channel. */

void route_cc(struct route *route, uintptr_t sel, uint8_t ctrl, uint8_t val) {
    union pattern_event ev = {
        .u8 = {
            PAT_MIDI_TAG(sel >> 4),
            0xB0 + (sel & 0xF),
            ctrl & 0x7f,
            val & 0x7f
        }
    };
    route_pattern_event(route, &ev);
    // FIXME: Record CC as well
}
void route_note(struct route *route, uintptr_t sel, uint8_t on_off, uint8_t note, uint8_t vel) {
    union pattern_event ev = {
        .u8 = {
            PAT_MIDI_TAG(sel >> 4),
            (on_off & 0xF0) + (sel & 0xF),
            note & 0x7f,
            vel & 0x7f
        }
    };
    route_pattern_event(route, &ev);

    struct app *app = route_to_app(route);
    struct mmc *mmc = &app->mmc;
    struct sequencer *s = &app->sequencer;

    // Recording
    if (mmc_record(mmc)) {
        /* The recorder is implemented in Erlang.

           Since traffic is one-way only, let's use a protocol that is
           convenient to parse at the Erlang side and easy to generate
           here: printed terms.  Is also easy to embed in sysex as
           ASCII. */

        if (mmc->running) {
            /* Send the event to the online recorder. */
            sequencer_cursor_write(s, &ev);
        }
        else {
            /* If the player is off, we send the events upstream. */
            to_erl_ptermf(
                "{record,{%d,<<%d,%d,%d,%d>>}}",
                mmc->time,
                ev.u8[0],
                ev.u8[1],
                ev.u8[2],
                ev.u8[3]);
        }
    }

}

static inline void process_uma_in(struct app *app) {
    // Just reuse the remote25 struct. Never used together.
    struct novation_remote *r = &app->novation_remote;
    struct route *route = &app->route;
    struct mmc *mmc = &app->mmc;

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
                route_note(route, r->sel, tag, note, vel);
                break;
            }
            case 0xB0: {
                /* Template 64 Zwizwa Exo has all knobs, sliders,
                   encoders mapped to CC in a linear fashion. */
                uint8_t cc = msg[1];
                uint8_t val = msg[2];
                if (cc == 25) {
                    mmc_stop(mmc);
                }
#if 0
                else if (cc == 26) {
                    // FIXME: what should this do?  Maybe just ignore
                    // because midi play/continue/stop is different.
                }
#endif
                else if (cc == 27) {
                    mmc_play(mmc);
                }
                else if (cc == 28) {
                    if (val == 0) {
                        LOG("rec on\n");
                        to_erl_pterm("{record,start}");
                        mmc_set_record(mmc, 1);
                    }
                    else {
                        LOG("rec off\n");
                        to_erl_pterm("{record,stop}");
                        mmc_set_record(mmc, 0);
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

    to_erl_flush();

}



static inline void process_novation_remote_in(struct app *app) {
    struct midi_cursor cur = midi_cursor_init(novation_remote_in, app->nframes);
    process_novation_remote(
        /* State */
        &app->novation_remote,
        /* Cursor into MIDI in buffer, MIDI from Novation Remote */
        &cur,
        /* Stateful local objects */
        &app->mmc,
        &app->sequencer,
        /* Remote uni-directional message targets. */
        &app->route
        );
}

static inline void process_arturia_minilab_in(struct app *app) {
    struct midi_cursor cur = midi_cursor_init(arturia_minilab_in, app->nframes);
    process_arturia_minilab(
        /* State */
        &app->arturia_minilab,
        /* Cursor into MIDI in buffer, MIDI from Novation Remote */
        &cur,
        /* Stateful local objects */
        &app->mmc,
        &app->sequencer,
        /* Remote uni-directional message targets. */
        &app->route
        );
}

static inline void process_maudio_axiom25_in(struct app *app) {
    struct midi_cursor cur = midi_cursor_init(maudio_axiom25_in, app->nframes);
    process_maudio_axiom25(
        /* State */
        &app->maudio_axiom25,
        /* Cursor into MIDI in buffer, MIDI from Novation Remote */
        &cur,
        /* Stateful local objects */
        &app->mmc,
        &app->sequencer,
        /* Remote uni-directional message targets. */
        &app->route
        );
}

struct hub_command;
struct hub_command {
    void (*fun)(struct telnet *);
    uintptr_t arg;
};

/* This could be more general.  Maybe best to also implement erlang
   commands using the jack ringbuffer */
static void process_hub_command(struct app *app, struct hub_command *c) {
    /* Re-using the signature from telnet_cmd */
    c->fun(&app->telnet);
}
static void process_telnet(struct app *app) {
    if (!app->telnet_ringbuffer) return;

#if 0
    // Leaving this here for later reference.  I guess this is the way
    // to do it if speed is important.  Got this from a2jmidid source.
    jack_ringbuffer_data_t vec[2];
    jack_ringbuffer_get_read_vector (app->telnet_ringbuffer, vec);
    for (int v=0; v<2; v++) {
        struct hub_command *c = (void*)vec[v].buf;
        int nb = vec[v].len / sizeof(*c);
        for (int i=0; i<nb; i++) {
            process_hub_command(app, c);
        }
    }
    // FIXME: Needs to advance still.
#else
    // We can just keep it simple.
    struct hub_command c;
    while (sizeof(c) == jack_ringbuffer_read(
               app->telnet_ringbuffer, (void*)&c, sizeof(c))) {
        process_hub_command(app, &c);
    }

#endif


}

static void app_process(struct app *app) {

    /* Erlang out is tagged with a rolling time stamp. */
    jack_nframes_t f = jack_last_frame_time(client);
    app->stamp = (f / app->nframes);

    /* Order is important. */
    process_telnet(app);
    process_clock_in(app);
    process_easycontrol_in(app);
    process_keystation_in1(app);
    process_keystation_in2(app);
    process_novation_remote_in(app);
    process_arturia_minilab_in(app);
    process_maudio_axiom25_in(app);
    process_uma_in(app);
    process_erl_out(app);

    /* FIXME: Normalize this. */
    void *akai_fire_in_buf = jack_port_get_buffer(akai_fire_in, app->nframes);
    akai_fire_process(&app->akai_fire, app->fire_out_buf, akai_fire_in_buf);

    process_z_debug(app);

}

static int process (jack_nframes_t nframes, void *arg) {
    struct app *app = &app_state;
    app->nframes = nframes;
    app->pd_out_buf    = midi_out_buf_cleared(pd_out, nframes);
    app->transport_buf = midi_out_buf_cleared(transport, nframes);
    app->fire_out_buf  = midi_out_buf_cleared(fire_out, nframes);
    app->synth_out_buf = midi_out_buf_cleared(synth_out, nframes);
    app_process(app);
    app->mmc.time += nframes;
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



#define CMD_CONNECT 1
#define CMD_DISCONNECT 2

int handle_jack_port(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, cmd, nsrc, ndst) {
        if (!((m->nsrc > 0) &&
              (m->ndst > 0) &&
              (m->nsrc + m->ndst == req->nb_bytes))) {
            LOG("bad size: nsrc=%d, ndst=%d, bytes=%d\n",
                m->nsrc, m->ndst, req->nb_bytes);
            return reply_error(req);
        }
        char src[m->nsrc + 1];
        char dst[m->ndst + 1];
        memcpy(src, req->bytes, m->nsrc);
        memcpy(dst, req->bytes + m->nsrc, m->ndst);
        src[m->nsrc] = 0;
        dst[m->ndst] = 0;
        switch(m->cmd) {
        case CMD_CONNECT:
            jack_connect(client, src, dst);
            return reply_ok(req);
        case CMD_DISCONNECT:
            jack_disconnect(client, src, dst);
            return reply_ok(req);
        default:
            LOG("bad command %d\n", m->cmd);
            return reply_error(req);
        }
    }
    return -1;
}

// Create two functions:
// One to get a list of patterns
// One to get a pattern's step sequence

// Still using the idea that complex data structures should be
// avoided. Complex refers to the code complexity of an
// encoder/decoder.  E.g. an array of a flat struct is ok, and because
// it covers a lot of ground it would be silly to omit. But larger
// (nested) structures are best split up into multiple calls.  It is
// going to be necessary to have locking anyway, so semantically
// mutiple calls work out just fine as a way to subdivide.

// Important here is to create a locking mechanism: during the
// traversal from the other thread, the data structure cannot be
// modified.
int handle_lock(struct tag_u32 *req) {
    return -1;
}
int handle_unlock(struct tag_u32 *req) {
    return -1;
}


int handle_list_patterns(struct tag_u32 *req) {
    struct app *app = req->context;
    struct sequencer *s = &app->sequencer;

    // ASSERT LOCKED

    /* The size information is implicit, so we count nb_patterns
       before we can allocate. */
    size_t nb_patterns = 0;
    FOR_SEQUENCER_PATTERNS(s, ip) {
        struct pattern_phase *pp = sequencer_pattern(s, ip.pattern_nb);
        if (pattern_phase_used == pattern_phase_lifecycle(pp)) {
            nb_patterns++;
        }
    }
    size_t nb_bytes = sizeof(pattern_t) * nb_patterns;
    pattern_t *pat = alloca(nb_bytes);
    size_t i = 0;

    /* Iterate a second time to record the patterns. */
    FOR_SEQUENCER_PATTERNS(s, ip) {
        struct pattern_phase *pp = sequencer_pattern(s, ip.pattern_nb);
        if (pattern_phase_used == pattern_phase_lifecycle(pp)) {
            pat[i++] = ip.pattern_nb;
        }
    }
    // FIXME: This could just as well send u32
    SEND_REPLY_TAG_U32_BYTES(req, (uint8_t*)pat, nb_bytes, 0 /* ok */);
    return 0;
}
struct pattern_step_ser {
    uint32_t u32;
    uint16_t delay;
} __attribute__((__packed__));
int handle_save_pattern(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, pattern_nb) {
        struct app *app = req->context;
        struct sequencer *s = &app->sequencer;
        struct pattern_phase *pp = sequencer_pattern(s, m->pattern_nb);
        if (pattern_phase_used != pattern_phase_lifecycle(pp)) {
            LOG("unused pattern %d\n", m->pattern_nb);
            return reply_error(req);
        }
        /* Iterate once to find size. */
        size_t nb_steps = 0;
        FOR_SEQUENCER_STEPS(s, m->pattern_nb, is) { nb_steps++; }

        size_t nb_bytes = sizeof(struct pattern_step_ser) * nb_steps;
        struct pattern_step_ser *step = alloca(nb_bytes);
        size_t i = 0;

        /* Then again to fill the array. */
        FOR_SEQUENCER_STEPS(s, m->pattern_nb, is) {
            step[i].u32 = is.step->event.u32;
            step[i].delay = is.step->delay;
            i++;
        }
        SEND_REPLY_TAG_U32_BYTES(req, (uint8_t*)step, nb_bytes, 0 /* ok */);
        return 0;
    }
    return -1;
}
int handle_load_pattern(struct tag_u32 *req) {
    struct app *app = req->context;
    struct sequencer *s = &app->sequencer;
    pattern_t pat_nb = sequencer_pattern_alloc(s);

    struct pattern_step_ser *step = (void*)req->bytes;
    size_t nb_steps = req->nb_bytes / sizeof(*step);
    for (size_t i = 0; i<nb_steps; i++) {
        union pattern_event ev = {.u32 = step[i].u32};
        sequencer_add_step_event(s, pat_nb, &ev, step[i].delay);
    }
    swtimer_schedule(&s->swtimer, 0, pat_nb);
    return reply_ok_1(req,pat_nb);
}

int handle_fire_update(struct tag_u32 *req) {
    struct app *app = req->context;
    app->akai_fire.need_update = 1;
    return reply_ok(req);
}
int handle_fire_button(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, row, col) {
        struct app *app = req->context;
        akai_fire_pad_event(&app->akai_fire, m->row, m->col);
        return reply_ok(req);
    }
    return -1;
}

/* Here "save" means from sequencer structure to tag_u32 return value,
   and "load" means tag_u32 argument to sequencer. */

int map_root(struct tag_u32 *req) {
    const struct tag_u32_entry map[] = {
        {"clock_div",     t_cmd, handle_clock_div, 1},
        {"jack_port",     t_cmd, handle_jack_port, 3},
        {"list_patterns", t_cmd, handle_list_patterns, 0},
        {"save_pattern",  t_cmd, handle_save_pattern, 1},
        {"load_pattern",  t_cmd, handle_load_pattern, 0},
        {"fire_update",   t_cmd, handle_fire_update, 0},
        {"fire_button",   t_cmd, handle_fire_button, 2},
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


/* Create/delete pattern turns on the LED on/off */
void app_pattern_state(struct sequencer *s, pattern_t pat, int state) {
    LOG("pattern %d alloc\n", pat);
    struct app *app = sequencer_to_app(s);
    int row = pat / 16;
    int col = pat % 16;
    app->akai_fire.pads[row][col] = state;
    app->akai_fire.need_update = 1;
}
void app_pattern_alloc_notify(struct sequencer *s, pattern_t pat) {
    app_pattern_state(s, pat, 1);
}
void app_pattern_free_notify(struct sequencer *s, pattern_t pat) {
    app_pattern_state(s, pat, 0);
}

/* Button press changes mute state.  If the pattern is not active, it
   doesn't do anything. */
void app_fire_button_notify(struct akai_fire *fire, int row, int col) {
    pattern_t pat = row * 16 + col;
    LOG("pattern %d mute toggle\n", pat);
    struct app *app = akai_fire_to_app(fire);
    struct pattern_phase *pp = sequencer_pattern(&app->sequencer, pat);
    if (pattern_phase_used == pattern_phase_lifecycle(pp)) {
        pp->mute ^= 1;
        app_pattern_state(&app->sequencer, pat, !pp->mute);
    }
}

void app_init(struct app *app) {
    /* Initialize the components. */
    akai_fire_init(&app->akai_fire);
    sequencer_init(&app->sequencer, app_sequencer_tick);
    sequencer_restart(&app->sequencer);

    /* Cross-link */
    app->sequencer.pattern_alloc_notify = app_pattern_alloc_notify;
    app->sequencer.pattern_free_notify = app_pattern_free_notify;
    app->akai_fire.button_notify = app_fire_button_notify;

    app->telnet_fd = -1;

}

void synth_tools_rs_init(void);
void synth_tools_zig_init(void);

#include "tcp_tools.h"
void telnet_write_output(struct telnet *t, const uint8_t *bytes, uintptr_t len) {
    struct app *app = telnet_to_app(t);
    assert_write(app->telnet_fd, bytes, len);
}

/* Telnet commands are resolved in the low prio thread, moved into a
   jack_ringbuffer then executed in the high prio thread. */
void play_pause(struct telnet *t) {
    struct app *app = telnet_to_app(t);
    mmc_toggle(&app->mmc);
}
struct telnet_cmd hub_cmds[] = {
    {"toggle", play_pause},
    {}
};
struct telnet_cmd hub_escs[] = {
    {"[11~", play_pause},
    {}
};

void telnet_ringbuffer_write(struct telnet *t, const struct telnet_cmd *c) {
    struct app *app = telnet_to_app(t);
    if (!c) return; // Much easier to make this a maybe
    struct hub_command cmd = { .fun = c->fun };
    if (sizeof(cmd) != jack_ringbuffer_write(
            app->telnet_ringbuffer,
            (void*)&cmd,
            sizeof(cmd))) {
        LOG("dropping telnet command\n");
    }
}

void telnet_event(struct telnet *t, uintptr_t event) {
    uint8_t byte = event & 0xFF;
    event &= ~0xff;
    switch(event) {
    case TELNET_EVENT_INTERRUPT:
        LOG("<INTERRUPT>\n");
        break;
    case TELNET_EVENT_CONTROL:
        LOG("<CONTROL:%d>\n", byte);
        if (byte == 4) {
            /* CTRL-D */
            /* Ignore for now. Figure out how to close the socket
             * without daemon restart. */
        }
        else if (byte == 12) { telnet_clear(t); }
        break;
    case TELNET_EVENT_ESCAPE:
        telnet_ringbuffer_write(t, telnet_escape_lookup(t, hub_escs));
        break;
    case TELNET_EVENT_LINE:
        // FIXME: number stack?
        telnet_ringbuffer_write(t,telnet_line_lookup(t, hub_cmds));
        break;
    case TELNET_EVENT_FLUSH:
        break;
    case TELNET_EVENT_PROMPT:
        t->write_output(t, (const uint8_t*)":", 2);
        break;
    }
}
void *telnet_main(void *arg) {
    struct app *app = arg;
    int listen_fd = assert_tcp_listen(12345);
    for (;;) {
        app->telnet_fd = assert_accept(listen_fd);
        app->telnet_ringbuffer = jack_ringbuffer_create(16 * sizeof(struct hub_command));
        telnet_init(&app->telnet, telnet_write_output, telnet_event);

        for(;;) {
            uint8_t buf[2048];
            ssize_t rv = read(app->telnet_fd, buf, sizeof(buf));
            if (rv == 0) break;
            ASSERT(rv >= 0);
            telnet_write_input(&app->telnet, buf, rv);
        }

        jack_ringbuffer_free(app->telnet_ringbuffer);
        app->telnet_ringbuffer = NULL;

    }
    return NULL;
}

int main(int argc, char **argv) {

    /* Initialize Rust and Zig libraries.  FIXME: This doesn't do
       anything except for making sure building and linking of Rust
       and Zig code works properly. */
    //synth_tools_rs_init();
    //synth_tools_zig_init();

    struct app *app = &app_state;
    app_init(app);

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

    /* Run a telnet server in the bac kground. */
    pthread_t telnet_thread;
    pthread_create(&telnet_thread, NULL, telnet_main, app);

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

