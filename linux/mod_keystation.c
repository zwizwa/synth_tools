struct keystation {
    uint8_t sel;
};

struct mmc;
void mmc_press_stop(struct mmc *mmc);
void mmc_press_play(struct mmc *mmc);
void mmc_press_record(struct mmc *mmc);

struct route;
void route_cc  (struct route *route, uintptr_t sel, uint8_t ctrl, uint8_t val);
void route_note(struct route *route, uintptr_t sel, uint8_t on_off, uint8_t note, uint8_t vel);

static inline void process_keystation_1(
    struct keystation *s,
    struct midi_cursor *events,
    struct mmc *mmc,
    struct route *route
)
{
    FOR_MIDI(event, events) {
        const uint8_t *msg = event->buffer;
        int n = event->size;
        /* Send a copy to Erlang.  FIXME: How to allocate midi port numbers? */
        to_erl_midi(msg, n, 1 /*midi port*/);

        uint8_t tag = msg[0] & 0xF0;

        if (n == 3) {
            switch(tag) {
            case 0x90: { /* Note on */
                uint8_t note = msg[1];
                uint8_t vel  = msg[2];
                switch(note) {
                case 0x59: // F
                    s->sel = 0;
                    break;
                case 0x5B: // G
                    s->sel = 0;
                    break;
                case 0x5D: // A
                    s->sel = 0;
                    break;
                case 0x5A: // F#
                    s->sel = 0;
                    break;
                case 0x5C: // G#
                    s->sel = 0;
                    break;
                case 0x5E: // A#
                    s->sel = 0;
                    break;
                default:
                    // FIXME: Route it to selected instrument
                    route_note(route, s->sel, tag, note, vel);
                    break;
                }
            }
            }
        }

    }
}
static inline void process_keystation_2(
    struct keystation *s,
    struct midi_cursor *events,
    struct mmc *mmc,
    struct route *route)
{
    FOR_MIDI(event, events) {
        const uint8_t *msg = event->buffer;
        int n = event->size;
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
                    mmc_press_play(mmc);
                    break;
                case 0x5d:
                    /* Stop press. */
                    LOG("keystation: stop\n");
                    mmc_press_stop(mmc);
                    break;
                }
#if 0
                /* Select different instrument. */
#endif
                break;
            }
            }
        }
    }
}
