#include <alsa/asoundlib.h>
#include "macros.h"

/* Just use globals for these. */
snd_seq_t *seq_handle;
snd_midi_event_t *alsa_decoder;

/* Docs say "negative error code".  Find out where to find them */
#define ALSA_ASSERT(cmd) \
    ({int err; if ((err=(cmd)) < 0) { ERROR("%s: ERROR %d\n", #cmd, err); }; err;})

int main(int argc, char **argv) {
    const char client_name[] = "tether_bl";
    ALSA_ASSERT(snd_seq_open(&seq_handle, "hw",
                             SND_SEQ_OPEN_OUTPUT | SND_SEQ_OPEN_INPUT, 0));
    snd_seq_set_client_name(seq_handle, client_name);

    /* Create ports */
    int out_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            seq_handle, "tether_bl_out",
            SND_SEQ_PORT_CAP_READ |
            SND_SEQ_PORT_CAP_SUBS_READ,
            SND_SEQ_PORT_TYPE_HARDWARE));
    LOG("out_id %d\n", out_id);

    int in_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            seq_handle, "tether_bl_in",
            SND_SEQ_PORT_CAP_WRITE |
            SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_HARDWARE));
    LOG("in_id %d\n", in_id);

    /* Create ALSA snd_seq_event_t decoder */
    ALSA_ASSERT(snd_midi_event_new(16, &alsa_decoder));
    snd_midi_event_reset_decode(alsa_decoder);
    snd_midi_event_no_status(alsa_decoder, 1);

    /* Get poll info */
    int npfd;
    struct pollfd *pfd;
    npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
    pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);

    for(;;) {
        // LOG("poll\n");
        if (poll(pfd, npfd, -1 /*inf*/) > 0) {
            do {
                snd_seq_event_t *ev;
                snd_seq_event_input(seq_handle, &ev);
                switch(ev->type) {
                case SND_SEQ_EVENT_PORT_SUBSCRIBED:
                    LOG("event: port subscribed\n");
                    break;
                default: {
                    /* Not handling all snd_seq_event_type separately.
                       Try to convert it to midi. */
                    static unsigned char buf[16];
                    long count = ALSA_ASSERT(
                        snd_midi_event_decode(
                            alsa_decoder, buf, sizeof(buf), ev));
                    if (count > 0) {
                        LOG("midi:");
                        for (long i=0; i<count; i++) { LOG(" %02x", buf[i]); }
                        LOG("\n");
                    }
                    else {
                        /* Event not supported or decoder error. */
                        LOG("WARNING: decode=%d, event=%d\n", count, ev->type);
                    }
                    snd_seq_free_event(ev);
                    break;
                }
                }
            } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
        }
    }

    return 0;
}
