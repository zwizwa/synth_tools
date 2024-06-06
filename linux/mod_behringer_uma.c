// FIXME: Not currently compiled.  Fix on next trip.

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

