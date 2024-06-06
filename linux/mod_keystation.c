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
