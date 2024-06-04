#ifndef MOD_NOVATION_REMOTE
#define MOD_NOVATION_REMOTE

/* Novation Remote 25 filter for recorder. */

/* The physical device is configured to address 8 different midi
   channels.  The mapping is performed by the code that glues this
   driver to the sequencer (which uses absolute addresses). */



/* Filter state. */
struct novation_remote {
    uint8_t sel;
};


/* Each module contains forward declarations for all the types used,
   all the methods, and all instances are passed as parameters.
   Functions are inline so most of that can be optimized away.

   This gives a close approximation to what "module" means in Racket,
   or "functor" in OCaml:
   - types and instances are black-box
   - interfaces are specified

*/

/* Convention: functions are not labeled static.  If functions need to
   be hidden or renamed, use a macro to redefine it before including
   the module file. */


/* This can be generalized.  Code below only uses FOR_MIDI and
   ->buffer, ->size dereferences. */

struct midi_cursor;

/* This is an "object" in the sense that it has a method that returns
   a result. */

struct mmc;
void mmc_play(struct mmc *mmc);
void mmc_stop(struct mmc *mmc);
void mmc_reset_time(struct mmc *mmc);
int mmc_running(struct mmc *mmc);
int mmc_record(struct mmc *mmc);
void mmc_set_record(struct mmc *mmc, int record);

/* All the other entitites are more abstract in that all
   methods are unidirectional. */

struct route;
void route_cc  (struct route *, uintptr_t sel, uint8_t ctrl,   uint8_t val);
void route_note(struct route *, uintptr_t sel, uint8_t on_off, uint8_t note, uint8_t vel);

void to_erl_midi(const uint8_t *buf, int nb, uint8_t port);
void to_erl_pterm(const char *pterm);


static void process_novation_remote(
    /* Private state data */
    struct novation_remote *s,
    /* MIDI in data is provided in a slightly general way.  This
       should be generalized more. */
    struct midi_cursor *events,
    /* Stateful local objects. */
    struct mmc *mmc,
    struct sequencer *seq,
    /* Remote uni-directional message targets. */
    struct route *route

) {

    FOR_MIDI(event, events) {
        const uint8_t *msg = event->buffer;
        int n = event->size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        uint8_t tag = msg[0];
        if (n == 3) {
            switch(tag) {
            case 0x80:
            case 0x90: {
                /* Route it to the current track. */
                uint8_t note = msg[1];
                uint8_t vel = msg[2];
                route_note(route, s->sel, tag, note, vel);
                break;
            }
            case 0xB0: {
                /* Template 64 Zwizwa Exo has all knobs, sliders,
                   encoders mapped to CC in a linear fashion. */
                uint8_t cc = msg[1];
                uint8_t val = msg[2];
                if (cc <= 7) {
                    uint8_t slider = cc;
                    s->sel = slider;
                    route_cc(route, s->sel, 0, val);
                }
                else if (cc <= 15) {
                    uint8_t slider_but = cc - 8;
                    s->sel = slider_but;
                    route_cc(route, s->sel, 1, val);
                }
                else if (cc <= 23) {
                    uint8_t knob = cc - 16;
                    s->sel = knob;
                    route_cc(route, s->sel, 2, val);
                }
                else if (cc <= 31) {
                    uint8_t knob_but = cc - 24;
                    s->sel = knob_but;
                    route_cc(route, s->sel, 3, val);
                }
                else if (cc <= 39) {
                    uint8_t rotary = cc - 32;
                    // local to s->sel
                    // FIXME: do rotary processing
                    route_cc(route, s->sel, 4 + rotary, val);
                }
                else if (cc <= 47) {
                    uint8_t rotary_but = cc - 40;
                    // local to s->sel
                    route_cc(route, s->sel, 4 + 8 + rotary_but, val);
                }
                else if (cc == 0x32) {
                    // stop
                    if (val == 0) {
                        if (mmc_record(mmc)) {
                            /* This is a special case for the
                               remote25, because pressing stop also
                               turns off recording. */
                            if (mmc_running(mmc)) {
                                LOG("live recorder stop (record->off)\n");
                                sequencer_cursor_close(seq);
                            }
                            else {
                                to_erl_pterm("{record,stop}");
                            }
                            mmc_set_record(mmc, 0);
                            mmc_stop(mmc);
                        }
                        else {
                            mmc_stop(mmc);
                        }
                    }
                }
                else if (cc == 0x33) {
                    if (val == 0) {
                        // play
                        if (mmc_record(mmc)) {
                            to_erl_pterm("{record,play}}");
                        }
                        else {
                            LOG("remote play->mmc_play\n");
                            mmc_play(mmc);
                        }
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
                        mmc_set_record(mmc, !mmc_record(mmc));
                        if (mmc_running(mmc)) {
                            /* If the player is on, we use the online
                               recorder. */
                            if (mmc_record(mmc)) {
                                dtime_t pat_len = 48; // FIXME
                                LOG("live recorder start, pat_len = %d\n", pat_len);
                                sequencer_cursor_open(seq, pat_len);
                            }
                            else {
                                LOG("live recorder stop (record->off)\n");
                                sequencer_cursor_close(seq);
                            }
                        }
                        else {
                            /* When recording is on but the playback
                               isn't, we send the events upstream for
                               processing and tempo + pattern
                               config. */
                            if (mmc_record(mmc)) {
                                mmc_reset_time(mmc);
                                to_erl_pterm("{record,start}");
                            }
                            else {
                                to_erl_pterm("{record,stop}");
                            }
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





#endif
