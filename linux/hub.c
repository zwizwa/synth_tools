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




/* See clock.c for comments.  Functionality is back inside hub.c for now */
#define BPM_TO_HPERIOD(sr,bpm) ((sr*5)/(bpm*4))


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1


#include "macros.h"
#include "jack_tools.h"
#include "assert_read.h"
#include "tag_u32.h"

#include "mod_sequencer.c"
#include "mod_akai_fire.c"
#include "mod_arturia_minilab.c"
#include "mod_maudio_axiom25.c"
#include "mod_keystation.c"

// FIXME: This needs to be rebuilt completely, so disable it for now.
// #include "mod_novation_remote.c"


#include "mod_to_erl.c"


#define TELNET_WORD_MODE
#include "mod_telnet.c"


#include "alsa_tools.h"


/* Since drivers are hardcoded in hub.c, the device port information
   should probably be hardcoded as well.  At startup and when a client
   port is created, the connections contained in this table are
   restored, and a routing entry is made for the specific device.

   To keep things simple, we relate everything to selectors.  The
   selectors represent a single MIDI channel, hosted on a specific
   port of a specific device.  Try to keep the map as flat as
   possible.

*/

#define MAX_NB_PORTS 6


struct alsa_dev {
    const char *name;
    uintptr_t from;    // we subscribe from this port mask
    uintptr_t to;      // we broadcast to this port mask
    uint8_t dev_id;    // device as we identify it internally
    uint8_t nb_ports;          // nb entries in sel
    uint8_t sel[MAX_NB_PORTS]; // port to selector map
};

const struct alsa_dev alsa_dev[] = {
    [0] = { .dev_id = 0, .name = "Axiom 25", .from = 0b111, .to = 0b1, .nb_ports = 3, .sel = {0,1,2} },
    [1] = { .dev_id = 1, .name = "synth",                   .to = 0b1, .nb_ports = 3, .sel = {3}     },
};

/* Try to keep it abstract.

   The important property is that each 'selector', e.g. each abstract
   midi channel, is referred to by name, and that name is a
   compile-time known entity in the C code.

   To be able to do that we also need to refer to devices and their
   driver by name.

   But that's it.

   Selector number, client id, device id, port, channel are all
   abstracted away behind those names.  Any other way is going to be
   unmanageable.  Flat namespaces are the way out.

*/



#include "mod_hub_devices.c"



void send_tag_u32_buf_write(const uint8_t *buf, uint32_t len) {
    uint8_t len_buf[4];
    write_be(len_buf, len, 4);
    assert_write(1, len_buf, 4);
    assert_write(1, buf, len);
}
#define SEND_TAG_U32_BUF_WRITE send_tag_u32_buf_write
#include "mod_send_tag_u32.c"

/* JACK */

/* For now all jack ports are disabled, replaced with ALSA */

#define FOR_MIDI_IN(m)

#define FOR_MIDI_IN_DISABLED(m) \
    m(clock_in)        \
    m(akai_fire_in)    \
    m(maudio_axiom25_in)        \
    m(easycontrol)     \
    m(arturia_minilab_in)      \
    m(keystation_in1)  \
    m(keystation_in2)  \
    m(z_debug)         \
    m(uma_in)          \
    m(novation_remote_in)          \


#define FOR_MIDI_OUT(m) \

#define FOR_MIDI_DISABLED(m) \
    m(tb03)         \
    m(fire_out)     \
    m(volca_keys)   \
    m(volca_bass)   \
    m(volca_beats)  \
    m(synth_out)    \
    m(pd_out)       \
    m(transport)    \

#define FOR_SELECTORS(m) \
    m(tb03,tb03,1)          \
    m(volca_keys,volcas,1)  \
    m(volca_bass,volcas,2)  \
    m(volca_beats,volcas,3) \


FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)

static jack_client_t *client = NULL;

struct mmc {
    uint32_t time;      /* rolling time */
    uint32_t mode:2;
    jack_nframes_t bpm;
    int clock_phase;
    int clock_pol;
    jack_nframes_t clock_hperiod;
};

struct route {
    /* We keep track of the notes we turned on so they can be turned
       off on stop.  Use a bit vector: 16 ports x 16 channels x 128
       notes is 32k bits is 4k bytes. */
    uintptr_t bitvec[4096 / sizeof(uintptr_t)];
};
#define BITSIZEOF(thing) (8*sizeof(thing))
static inline void route_set_bit(struct route *r, uintptr_t bit_nb) {
    uintptr_t byte_nb = bit_nb / BITSIZEOF(r->bitvec[0]);
    bit_nb %= BITSIZEOF(r->bitvec[0]);
    ASSERT(byte_nb < ARRAY_SIZE(r->bitvec));
    r->bitvec[byte_nb] |= (1 << bit_nb);
}
static inline void route_clear_bit(struct route *r, uintptr_t bit_nb) {
    uintptr_t byte_nb = bit_nb / BITSIZEOF(r->bitvec[0]);
    bit_nb %= BITSIZEOF(r->bitvec[0]);
    ASSERT(byte_nb < ARRAY_SIZE(r->bitvec));
    r->bitvec[byte_nb] &= ~(1 << bit_nb);
}
static inline int route_read_bit(struct route *r, uintptr_t bit_nb) {
    uintptr_t byte_nb = bit_nb / BITSIZEOF(r->bitvec[0]);
    bit_nb %= BITSIZEOF(r->bitvec[0]);
    ASSERT(byte_nb < ARRAY_SIZE(r->bitvec));
    return !!(r->bitvec[byte_nb] & (1 << bit_nb));
}
static inline uintptr_t pattern_event_bit_nb(const union pattern_event *ev) {
    // Precondition: this is a midi event.
    uintptr_t port = ev->u8[0] & 0x0F;
    uintptr_t chan = ev->u8[1] & 0x0F;
    uintptr_t note = ev->u8[2] & 0x7F;
    uintptr_t bit_nb = (((port << 4) + chan) << 7) + note;
    return bit_nb;
}


struct pd { };
struct synth { };

// figure out how to map struct to parent


struct app {
    struct sequencer sequencer;
    jack_nframes_t nframes;
    jack_nframes_t sr;
    uint8_t stamp;

    /* message handler state */
    struct route route;
    // struct novation_remote novation_remote;
    struct arturia_minilab arturia_minilab;
    struct maudio_axiom25 maudio_axiom25;
    struct keystation keystation;
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

    /* ALSA MIDI */
    snd_seq_t *seq;
    snd_midi_event_t *alsa_decoder;
    snd_midi_event_t *alsa_encoder;
    int alsa_queue_id;
    int alsa_client_id;
    int alsa_port_id;
    pthread_t alsa_thread;
    int alsa_pipefd[2]; //0=read, 1=write

    /* Map live ALSA client id to our representation of the device. */
    uint8_t client_id_to_dev_id[256];

} app_state = {};

#define PORT_ID_NONE 255
#define DEV_ID_NONE 255






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




/* This defines the main start/stop/record state machine such that
   drivers for the individual midi controllers just need to map
   keypresses to actions.

   There are essentially two recording modes:

   1. Live record mode, where the sequencer is playing back events,
      and additional events are recorded in additional patterns.

   2. Offline record mode, where the sequencer is not playing back
      events, and an initial rhythm is sent upstream to Erlang code
      where it is processed and converted to temp + initial pattern.

   In addition, there is full off and playback.

   playing x recording

   0         0           off
   1         0           playback
   1         1           live record
   0         1           offline record

   It seems simplest to represent the code as handling transitions
   between these 4 states, instead of the product of playing and
   recording.
*/

#define MMC_MODE_OFF         0
#define MMC_MODE_PLAY        1
#define MMC_MODE_OFFLINE_REC 2
#define MMC_MODE_LIVE_REC    3


void mmc_reset_time(struct mmc *mmc) {
    mmc->time = 0;
}
int mmc_running(struct mmc *mmc) {
    return mmc->mode & 1;
}

void route_pattern_event(struct route *route, const union pattern_event *ev);


/* Provide only the 3 transition functions, ignoring sequences that
   make no sense. */
void mmc_press_stop(struct mmc *mmc) {
    uintptr_t prev_mode = mmc->mode;
    struct app *app = mmc_to_app(mmc);
    struct route *r = &app->route;
    switch(mmc->mode) {
    case MMC_MODE_OFF:
        /* Already off. Ignore. */
        break;
    case MMC_MODE_PLAY:
        /* Turn off any slave devices and stop sequencer. */
        // FIXME: send_stop(app->transport_buf);

        sequencer_restart(&app->sequencer);
        mmc->mode = MMC_MODE_OFF;
        /* Send note off events for all notes that are still on. */
        for (uintptr_t port = 0; port< 16; port++) {
        for (uintptr_t chan = 0; chan< 16; chan++) {
        for (uintptr_t note = 0; note<128; note++) {
            union pattern_event ev = {
                .u8 = { PAT_MIDI_TAG(port), 0x80 + chan, note, 0 }
            };
            if (route_read_bit(r, pattern_event_bit_nb(&ev))) {
                LOG("STOP: port=%d chan=%d note=%d OFF\n", port, chan, note);
                route_pattern_event(r, &ev);
            }
        }}}
        memset(r->bitvec, 0, sizeof(r->bitvec));
        break;
    case MMC_MODE_OFFLINE_REC:
        /* Stop upstream recorder. */
        to_erl_pterm("{record,stop}");
        mmc->mode = MMC_MODE_OFF;
        break;
    case MMC_MODE_LIVE_REC:
        /* Turn off both recording and playback. */
        sequencer_cursor_close(&app->sequencer);
        mmc->mode = MMC_MODE_OFF;
        break;
    }
    LOG("STOP: mode: %d->%d\n", prev_mode, mmc->mode);

    /* FIXME: Iterate over all notes that are left on and turn them
       off. */

}
void mmc_press_play(struct mmc *mmc) {
    uintptr_t prev_mode = mmc->mode;
    switch(mmc->mode) {
    case MMC_MODE_OFF:
        /* Enable sequencer. */
        mmc->mode = MMC_MODE_PLAY;
        break;
    case MMC_MODE_PLAY:
        /* Already playing. Ignore */
        break;
    case MMC_MODE_OFFLINE_REC:
        /* Doesn't make sense in offline recording, so ignore. */
        break;
    case MMC_MODE_LIVE_REC:
        /* Doesn't make sense in live recording, so ignore. */
        break;
    }
    LOG("PLAY: mode: %d->%d\n", prev_mode, mmc->mode);
}
void mmc_press_record(struct mmc *mmc) {
    uintptr_t prev_mode = mmc->mode;
    struct app *app = mmc_to_app(mmc);
    switch(mmc->mode) {
    case MMC_MODE_OFF:
        /* Start offline recorder. */
        mmc_reset_time(mmc);
        to_erl_pterm("{record,start}");
        mmc->mode = MMC_MODE_OFFLINE_REC;
        break;
    case MMC_MODE_PLAY: {
        /* Start live recorder. */
        dtime_t pat_len = 48; // FIXME
        LOG("live recorder start, pat_len = %d\n", pat_len);
        sequencer_cursor_open(&app->sequencer, pat_len);
        mmc->mode = MMC_MODE_LIVE_REC;
        break;
    }
    case MMC_MODE_OFFLINE_REC:
        /* Turn off offline recording mode. */
        to_erl_pterm("{record,stop}");
        mmc->mode = MMC_MODE_OFF;
        break;
    case MMC_MODE_LIVE_REC:
        /* Turn off live recording mode.  Note that this really needs
           to be a separate button because stop button will stop the
           playback as well. */
        sequencer_cursor_close(&app->sequencer);
        mmc->mode = MMC_MODE_PLAY;
        break;
    }
    LOG("RECORD: mode: %d->%d\n", prev_mode, mmc->mode);

}










#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))

static inline void *midi_out_buf_cleared(jack_port_t *port, jack_nframes_t nframes) {
    void *buf = jack_port_get_buffer(port, nframes);
    jack_midi_clear_buffer(buf);
    return buf;
}



#if 0
static inline void process_z_debug(struct app *app) {
    FOR_MIDI_EVENTS(iter, z_debug, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        int n = iter.event.size;
        LOG_HEX("z_debug:", msg, n);
    }
}
#endif


#if 0
static inline void process_clock_in(struct app *app) {
    struct mmc *mmc = &app->mmc;
    FOR_MIDI_EVENTS(iter, clock_in, app->nframes) {
        const uint8_t *msg = iter.event.buffer;
        if (iter.event.size == 1) {
            switch(msg[0]) {
            case 0xFB: // continue FIXME: This is wrong
            case 0xFA: // start
                LOG("clock_in start->app_play\n");
                mmc_press_play(mmc);
                break;
            case 0xFC: // stop
                mmc_press_stop(mmc);
                break;
            case 0xF8: { // clock
                // LOG("tick, running=%d\n", app->running);
                if (mmc_running(mmc)) {
                    sequencer_tick(&app->sequencer);
                }
                break;
            }
            }
        }
    }
}
#endif

// FIXME: I want a simpler midi dispatch construct.

#if 0
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
#endif


/* Data flow:  FIXME TODO

   - The midi drivers think in terms of midi messages and 'selectors',
     which are like channels but have a larger span.

   - We convert that data to pattern_event

   - It is the pattern_event data that is routed

*/


/* Map selector to port/channel. */

/* Note that this is now only called in the ALSA thread.  Jack devices
   can only be reached by writing to a ring buffer and then performing
   the write in the jack process function, which is currently not
   supported. */

void route_pattern_event(struct route *route, const union pattern_event *ev) {

    const uint8_t *msg = &ev->u8[1];
    int len = 3; // FIXME: Depends on the contents of the event.  Currently only note, cc.

    struct app *app = route_to_app(route);
    (void)app;
    int port = ev->u8[0] & 0x0F; // FIXME: Assumes midi

    /* Below is only for MIDI */

    /* Keep track of what we turned on and off. */
    uint8_t midi_cmd = ev->u8[1] & 0xF0;
    if ((midi_cmd == 0x90) || (midi_cmd == 0x80)) {
        uintptr_t bit_nb = pattern_event_bit_nb(ev);
        if (midi_cmd == 0x90) {
            route_set_bit(route, bit_nb);
        }
        else {
            /* Not clearing these makes it a little more robust in
               case we missed recording a note off event. */
            // route_clear_bit(route, bit_nb);
        }
    }

    switch(port) {
    /* Jack midi port connected to pd_io object, which takes jack
       midi in and converts it to netsend into Pd. */
    case 0:
        // FIXME: send_midi(app->pd_out_buf, 0, msg, len);
        break;
    /* synth.c */
    case 1:
        // FIXME: send_midi(app->synth_out_buf, 0, msg, len);
        break;
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
            /* Map 'sel' to port, chan for now. Later this should
               probably be more general, e.g. allow the 256 virtual
               channels be spread over more ports.  It is quite common
               to have ports that use only one channel. */
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
    switch(mmc->mode) {
    case MMC_MODE_OFFLINE_REC:
        to_erl_ptermf(
            "{record,{%d,<<%d,%d,%d,%d>>}}",
            mmc->time,
            ev.u8[0],
            ev.u8[1],
            ev.u8[2],
            ev.u8[3]);
        break;
    case MMC_MODE_LIVE_REC:
        sequencer_cursor_write(s, &ev);
        LOG("sequencer_cursor_write %02x %02x %02x %02x\n",
            ev.u8[0], ev.u8[1], ev.u8[2], ev.u8[3]);
        break;
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



#if 0
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
#endif

#if 0
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
#endif

#if 0
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
#endif

#if 0
static inline void process_keystation_in1(struct app *app) {
    struct midi_cursor cur = midi_cursor_init(keystation_in1, app->nframes);
    process_keystation_1(
        /* State */
        &app->keystation,
        /* Cursor into MIDI in buffer, MIDI from Novation Remote */
        &cur,
        /* Stateful local objects */
        &app->mmc,
        /* Remote uni-directional message targets. */
        &app->route
        );
}
#endif

#if 0
static inline void process_keystation_in2(struct app *app) {
    struct midi_cursor cur = midi_cursor_init(keystation_in2, app->nframes);
    process_keystation_2(
        /* State */
        &app->keystation,
        /* Cursor into MIDI in buffer, MIDI from Novation Remote */
        &cur,
        /* Stateful local objects */
        &app->mmc,
        /* Remote uni-directional message targets. */
        &app->route
        );
}
#endif


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

void app_to_alsa(struct app *app, uintptr_t ev) {
    assert_write(app->alsa_pipefd[1], (const void*)&ev, sizeof(ev));
}

static void process_clock(struct app *app) {
    /* This is an integer divisor of the sample clock, which makes it
       possible to have perfect lock for devices that only receive
       word clock in. */
    struct mmc *mmc = &app->mmc;
    if (!mmc->clock_hperiod) {
        /* It seems that sr is only valid in side the process thread,
           so set it here once. */
        mmc->clock_hperiod = BPM_TO_HPERIOD(app->sr, mmc->bpm);
        float bpm_actual = (((float)app->sr)*1.25f) / ((float)mmc->clock_hperiod);
        LOG("clock: bpm_set = %d, sr = %d -> clock_hperiod = %d, bpm_actual = %3.6f\n",
            mmc->bpm, app->sr, mmc->clock_hperiod, bpm_actual);
        // FIXME: Send out an NRPN as well?
    }

    /* Generate quare wave output, send midi on positive edge. */
    for (int t=0; t<app->nframes; t++) {
        if (mmc->clock_phase >= mmc->clock_hperiod) {
            mmc->clock_phase -= mmc->clock_hperiod;
            mmc->clock_pol ^= 1;
            if (mmc->clock_pol == 1) {
                // Send event to ALSA thread
                uintptr_t ev = 0;
                app_to_alsa(app, ev);
                // const uint8_t clock[] = {0xF8};
                // send_midi(midi_out_buf, t, clock, sizeof(clock));
            }
        }
        // FIXME: Write audio sample later when adding audio port
        // audio_out_buf[t] = clock_pol;
        mmc->clock_phase += 1;
    }
}

static void app_process(struct app *app) {

    /* Erlang out is tagged with a rolling time stamp. */
    jack_nframes_t f = jack_last_frame_time(client);
    app->sr = jack_get_sample_rate(client);

    app->stamp = (f / app->nframes);

    /* Order is important. */
    process_telnet(app);

    // process_clock_in(app);
    // process_easycontrol_in(app);
    // process_arturia_minilab_in(app);
    // process_maudio_axiom25_in(app);
    // process_keystation_in1(app);
    // process_keystation_in2(app);
    // process_novation_remote_in(app);
    // process_uma_in(app);
    process_erl_out(app);

    /* FIXME: Normalize this. */
    //void *akai_fire_in_buf = jack_port_get_buffer(akai_fire_in, app->nframes);
    //akai_fire_process(&app->akai_fire, app->fire_out_buf, akai_fire_in_buf);

    // process_z_debug(app);


    process_clock(app);

    //uintptr_t ev = 0;
    //app_to_alsa(app, ev);


}

static int process (jack_nframes_t nframes, void *arg) {
    struct app *app = &app_state;
    app->nframes = nframes;

    // app->pd_out_buf    = midi_out_buf_cleared(pd_out, nframes);
    // app->transport_buf = midi_out_buf_cleared(transport, nframes);
    // app->fire_out_buf  = midi_out_buf_cleared(fire_out, nframes);
    // app->synth_out_buf = midi_out_buf_cleared(synth_out, nframes);

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

    memset(app, 0, sizeof(*app));
    memset(app->client_id_to_dev_id, DEV_ID_NONE, sizeof(app->client_id_to_dev_id));

    /* Initialize the components. */
    akai_fire_init(&app->akai_fire);
    sequencer_init(&app->sequencer, app_sequencer_tick);
    sequencer_restart(&app->sequencer);

    /* Cross-link */
    app->sequencer.pattern_alloc_notify = app_pattern_alloc_notify;
    app->sequencer.pattern_free_notify = app_pattern_free_notify;
    app->akai_fire.button_notify = app_fire_button_notify;

    app->telnet_fd = -1;

    app->mmc.bpm = 120;
    app->mmc.clock_phase = 0;
    app->mmc.clock_pol = 1;

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
    struct mmc *mmc = &app->mmc;
    if (mmc_running(mmc)) {
        mmc_press_stop(mmc);
    }
    else {
        mmc_press_play(mmc);
    }
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

#define MAX_MIDI 1024

void app_alsa_connect_input(struct app *app, uint8_t client, uint8_t port) {
    snd_seq_addr_t src = { .client = client, .port = port};
    snd_seq_addr_t dst = { .client = app->alsa_client_id, .port = app->alsa_port_id};
    alsa_connect(app->seq, src, dst);
}
void app_alsa_connect_output(struct app *app, uint8_t client, uint8_t port) {
    snd_seq_addr_t src = { .client = app->alsa_client_id, .port = app->alsa_port_id};
    snd_seq_addr_t dst = { .client = client, .port = port};
    alsa_connect(app->seq, src, dst);
}

void app_alsa_maybe_connect(struct app *app,
                            const struct alsa_dev *dev,
                            int nb_dev,
                            const char *client_name, uint8_t client_id) {
    for(int i=0; i<nb_dev; i++) {
        const struct alsa_dev *d = &dev[i];
        if (!strcmp(d->name, client_name)) {
            uintptr_t fromputs = d->from;
            for(int i=0; fromputs!=0; i++,fromputs>>=1) {
                if (fromputs & 1) {
                    app_alsa_connect_input(app, client_id, i);
                }
            }
            uintptr_t toputs = d->to;
            for(int i=0; toputs!=0; i++,toputs>>=1) {
                if (toputs & 1) {
                    app_alsa_connect_output(app, client_id, i);
                }
            }
            return;
        }
    }
}

void app_alsa_connect(struct app *app,
                      const struct alsa_dev *dev,
                      int nb_dev) {
    snd_seq_client_info_t *cinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(app->seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);
        const char *name = snd_seq_client_info_get_name(cinfo);
        int nb_ports = snd_seq_client_info_get_num_ports(cinfo);
        LOG("client %d %s:%d\n", client, name, nb_ports);
        app_alsa_maybe_connect(app, dev, nb_dev, name, client);
    }
}

void handle_axiom25_0_0(struct app *app, const uint8_t *buf, int count) {}
void handle_axiom25_1_0(struct app *app, const uint8_t *buf, int count) {}
void handle_axiom25_2_0(struct app *app, const uint8_t *buf, int count) {}
void handle_synth(struct app *app, const uint8_t *buf, int count) {}

typedef void (*app_midi_fn)(struct app *app, const uint8_t *buf, int count);
#define MIDI_HANDLE(name,dev,port,chan) handle_##name,
const app_midi_fn app_midi_handle[] = {
    FOR_SEL(MIDI_HANDLE)
};

app_midi_fn route_dpc(struct app *app, uint8_t client, uint8_t port, uint8_t chan) {
    /* Dynamic mapping: ALSA client to our internal dev_id */
    uint8_t dev_id = app->client_id_to_dev_id[client];

    /* With dev_id the static maps can be used to find dev, then sel
       based on port number, then handler based on selector. */
    if (dev_id == DEV_ID_NONE) return NULL;
    ASSERT(dev_id < ARRAY_SIZE(alsa_dev));
    const struct alsa_dev *d = &alsa_dev[dev_id];
    ASSERT(d->nb_ports <= MAX_NB_PORTS);
    ASSERT(port < d->nb_ports);

    intptr_t sel = dpc_to_sel(dev_id, port, chan);
    if ((sel >= 0) &&
        (sel < ARRAY_SIZE(app_midi_handle))) {
        return app_midi_handle[sel];
    }
    return NULL;
}

void app_route_midi_incoming(struct app *app,
                             snd_seq_addr_t addr,
                             const uint8_t *buf, int count) {

    uint8_t client = addr.client;
    uint8_t port   = addr.port;
    uint8_t chan   = buf[0] & 0x0F; // FIXME

    app_midi_fn fn = route_dpc(app, client, port, chan);
    if (fn) {
        fn(app, buf, count);
    }

#if 0
    /* Re-encode */
    snd_seq_event_t out_ev;
    snd_seq_ev_clear(&out_ev);
    if (snd_midi_event_encode(
            app->alsa_encoder, buf, count, &out_ev)) {
        snd_seq_ev_set_source(&out_ev, app->alsa_port_id);
        snd_seq_ev_set_subs(&out_ev);
        snd_seq_ev_schedule_tick(&out_ev, app->alsa_queue_id, 1, 0);
        snd_seq_event_output_direct(app->seq, &out_ev);
    }
#endif

}

void *alsa_main(void *arg) {
    struct app *app = arg;

    int npfd = snd_seq_poll_descriptors_count(app->seq, POLLIN);
    struct pollfd pfd[npfd + 1]; // One extra for pipe
    snd_seq_poll_descriptors(app->seq, pfd + 1, npfd, POLLIN);

    pfd[0].fd = app->alsa_pipefd[0];
    pfd[0].events = POLLIN;

    for(;;) {

        // LOG("poll\n");
        if (poll(pfd, 1 + npfd, -1 /*inf*/) > 0) {

            if(pfd[0].revents & POLLIN) {
                /* Event from jack thread. */
                uintptr_t ev;
                assert_read(pfd[0].fd, &ev, sizeof(ev));
                switch(ev) {
                case 0: {
                    if (mmc_running(&app->mmc)) {
                        sequencer_tick(&app->sequencer);
                    }

                    // LOG("MIDI CLOCK\n");
                    snd_seq_event_t clock_ev;
                    uint8_t midi_clock[1] = { 0xF8 };
                    snd_seq_ev_clear(&clock_ev);
                    if (snd_midi_event_encode(
                            app->alsa_encoder,
                            midi_clock, sizeof(midi_clock),
                            &clock_ev)) {
                        snd_seq_ev_set_source(&clock_ev, app->alsa_port_id);
                        snd_seq_ev_set_subs(&clock_ev);
                        snd_seq_ev_schedule_tick(&clock_ev, app->alsa_queue_id, 1, 0);
                    }
                    snd_seq_event_output_direct(app->seq, &clock_ev);
                    break;
                }
                }
                // LOG("jack event\n");
            }
            else do {
                snd_seq_event_t *ev;
                snd_seq_event_input(app->seq, &ev);

                if (ev->source.client == 0) {
                    /* System messages. */
                    switch(ev->type) {
#if 0
                    case SND_SEQ_EVENT_PORT_SUBSCRIBED: {
                        snd_seq_connect_t *c = &ev->data.connect;
                        LOG("event: port subscribed %d:%d -> %d:%d\n",
                            c->sender.client, c->sender.port,
                            c->dest.client, c->dest.port);
                        break;
                    }
                    case SND_SEQ_EVENT_PORT_UNSUBSCRIBED: {
                        snd_seq_connect_t *c = &ev->data.connect;
                        LOG("event: port unsubscribed %d:%d -> %d:%d\n",
                            c->sender.client, c->sender.port,
                            c->dest.client, c->dest.port);
                        break;
                    }
                    case SND_SEQ_EVENT_CLIENT_EXIT: {
                        snd_seq_addr_t *a = &ev->data.addr;
                        LOG("event: client exit %d:%d\n",
                            a->client, a->port);
                        break;
                    }
#endif
                    case SND_SEQ_EVENT_PORT_START: {
                        snd_seq_addr_t *a = &ev->data.addr;
                        LOG("event: port start %d:%d\n",
                            a->client, a->port);
                        /* Do the stupid thing and just rerun the
                           connection routine. */
                        app_alsa_connect(app, alsa_dev, ARRAY_SIZE(alsa_dev));
                        break;
                    }
                    }
                }
                else {
                    switch(ev->type) {
                    default: {
                        /* Not handling all snd_seq_event_type separately.
                           Try to convert it to midi. */
                        static unsigned char buf[MAX_MIDI];
                        long count = ALSA_ASSERT(
                            snd_midi_event_decode(
                                app->alsa_decoder, buf, sizeof(buf), ev));
                        if (count > 0) {
                            if (1) {
                                LOG("ALSA MIDI %d:%d", ev->source.client, ev->source.port);
                                for (long i=0; i<count; i++) { LOG(" %02x", buf[i]); }
                                LOG("\n");
                            }
                            app_route_midi_incoming(app, ev->source, buf, count);
                        }
                        else {
                            /* Event not supported or decoder error. */
                            LOG("WARNING: decode=%d, event=%d\n", count, ev->type);
                        }
                        break;
                    }
                    }
                }
                snd_seq_free_event(ev);

            } while (snd_seq_event_input_pending(app->seq, 0) > 0);


        }

    }
}


void app_alsa_init(struct app *app) {
    ALSA_ASSERT(snd_seq_open(&app->seq, "hw",
                             SND_SEQ_OPEN_OUTPUT | SND_SEQ_OPEN_INPUT, 0));
    snd_seq_set_client_name(app->seq, "hub");

    app->alsa_client_id = snd_seq_client_id(app->seq);

    app->alsa_queue_id = ALSA_ASSERT(snd_seq_alloc_queue(app->seq));

    app->alsa_port_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            app->seq, "hub_midi",
            SND_SEQ_PORT_CAP_READ |
            SND_SEQ_PORT_CAP_SUBS_READ |
            SND_SEQ_PORT_CAP_WRITE |
            SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_HARDWARE));

    ALSA_ASSERT(snd_midi_event_new(MAX_MIDI, &app->alsa_decoder));
    snd_midi_event_reset_decode(app->alsa_decoder);
    snd_midi_event_no_status(app->alsa_decoder, 1);

    ALSA_ASSERT(snd_midi_event_new(MAX_MIDI, &app->alsa_encoder));

    /* System messages. */
    snd_seq_addr_t self = { .client = app->alsa_client_id, .port = app->alsa_port_id};
    snd_seq_addr_t announce = { .client = 0, .port = 1};
    alsa_connect(app->seq, announce, self);

    /* Connect to hardware ports for which we have drivers. */
    app_alsa_connect(app, alsa_dev, ARRAY_SIZE(alsa_dev));
 
    /* Events from jack realtime thread to low-pri ALSA thread */
    ASSERT_ERRNO(pipe(app->alsa_pipefd));

    /* Run ALSA I/O in the backkground. */
    pthread_create(&app->alsa_thread, NULL, alsa_main, app);

}

int main(int argc, char **argv) {

    /* Initialize Rust and Zig libraries.  FIXME: This doesn't do
       anything except for making sure building and linking of Rust
       and Zig code works properly. */
    //synth_tools_rs_init();
    //synth_tools_zig_init();

    struct app *app = &app_state;
    app_init(app);

    /* ALSA MIDI setup */
    app_alsa_init(app);

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

