#ifndef MOD_MAUDIO_AXIOM25
#define MOD_MAUDIO_AXIOM25

/* Novation Remote 25 filter for recorder. */

/* Filter state. */
struct maudio_axiom25 {
    uint8_t sel;
    uint8_t record;
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

/* All the other entitites are more abstract in that all
   methods are unidirectional. */

struct route;
void route_cc  (struct route *route, uintptr_t sel, uint8_t ctrl, uint8_t val);
void route_note(struct route *route, uintptr_t sel, uint8_t on_off, uint8_t note, uint8_t vel);

void to_erl_midi(const uint8_t *buf, int nb, uint8_t port);
void to_erl_pterm(const char *pterm);


static void process_maudio_axiom25(
    /* Private state data */
    struct maudio_axiom25 *s,
    /* MIDI in data is provided in a slightly general way.  This
       should be generalized more. */
    struct midi_cursor *events,
    /* Stateful local objects. */
    struct mmc *mmc,
    struct sequencer *seq,
    /* Remote uni-directional message targets. */
    struct route *route

) {

    uintptr_t sel = 16; // FIXME hardcoded

    FOR_MIDI(event, events) {
        const uint8_t *msg = event->buffer;
        int n = event->size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        uint8_t tag = msg[0];
        if (n == 3) {
            switch(tag) {
            case 0x80:
            case 0x90: {
                /* Route it to route. */
                uint8_t note = msg[1];
                uint8_t vel = msg[2];
                route_note(route, sel, tag, note, vel);
                break;
            }
            case 0xB0: {
                uint8_t cc = msg[1];
                uint8_t val = msg[2];
                switch(cc) {
                }
                route_cc(route, sel, cc, val);
                break;
            case 0xBF: {
                uint8_t cc = msg[1];
                uint8_t val = msg[2];
                if (val == 0x7F) {
                    switch(cc) {
                    case 0x74:
                        LOG("stop\n");
                        break;
                    case 0x75:
                        LOG("play\n");
                        break;
                    case 0x76:
                        LOG("record\n");
                        break;
                    }
                }
            }
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
