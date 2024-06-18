#include "macros.h"
#include "alsa_tools.h"

/* Just use globals for these. */
snd_seq_t *seq_handle;
snd_midi_event_t *alsa_decoder;
snd_midi_event_t *alsa_encoder;
int queue_id;

#define MAX_MIDI 1024


void list_clients(void) {
    snd_seq_client_info_t *cinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq_handle, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);
        const char *name = snd_seq_client_info_get_name(cinfo);
        int nb_ports = snd_seq_client_info_get_num_ports(cinfo);
        LOG("client %d %s:%d\n", client, name, nb_ports);
    }
}

#if 0
void connect_client(const char *sender_str, const char *dest_str) {
    snd_seq_addr_t sender, dest;
    ALSA_ASSERT(snd_seq_parse_address(seq_handle, &sender, sender_str));
    ALSA_ASSERT(snd_seq_parse_address(seq_handle, &dest, dest_str));
}
#endif

int main(int argc, char **argv) {
    const char client_name[] = "tether_bl";
    ALSA_ASSERT(snd_seq_open(&seq_handle, "hw",
                             SND_SEQ_OPEN_OUTPUT | SND_SEQ_OPEN_INPUT, 0));
    snd_seq_set_client_name(seq_handle, client_name);
    queue_id = ALSA_ASSERT(snd_seq_alloc_queue(seq_handle));


    // FIXME: How to subscribe to an existing client?

    /* Create ports */
    int out_port_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            seq_handle, "tether_bl_out",
            SND_SEQ_PORT_CAP_READ |
            SND_SEQ_PORT_CAP_SUBS_READ,
            SND_SEQ_PORT_TYPE_HARDWARE));
    //LOG("out_port_id %d\n", out_port_id);

    int in_port_id = ALSA_ASSERT(
        snd_seq_create_simple_port(
            seq_handle, "tether_bl_in",
            SND_SEQ_PORT_CAP_WRITE |
            SND_SEQ_PORT_CAP_SUBS_WRITE,
            SND_SEQ_PORT_TYPE_HARDWARE));
    //LOG("in_port_id %d\n", in_port_id);
    (void)in_port_id;

    /* Create ALSA snd_seq_event_t decoder */
    ALSA_ASSERT(snd_midi_event_new(MAX_MIDI, &alsa_decoder));
    snd_midi_event_reset_decode(alsa_decoder);
    snd_midi_event_no_status(alsa_decoder, 1);

    ALSA_ASSERT(snd_midi_event_new(MAX_MIDI, &alsa_encoder));


    list_clients();



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
                    // LOG("event: port subscribed\n");
                    break;
                default: {
                    /* Not handling all snd_seq_event_type separately.
                       Try to convert it to midi. */
                    static unsigned char buf[MAX_MIDI];
                    long count = ALSA_ASSERT(
                        snd_midi_event_decode(
                            alsa_decoder, buf, sizeof(buf), ev));
                    if (count > 0) {
                        LOG("midi:");
                        for (long i=0; i<count; i++) { LOG(" %02x", buf[i]); }
                        LOG("\n");

                        /* Re-encode */
                        snd_seq_event_t out_ev;
                        snd_seq_ev_clear(&out_ev);
                        if (snd_midi_event_encode(
                                alsa_encoder, buf, count, &out_ev)) {
                            snd_seq_ev_set_source(&out_ev, out_port_id);
                            snd_seq_ev_set_subs(&out_ev);
                            snd_seq_ev_schedule_tick(&out_ev, queue_id, 1, 0);
                            snd_seq_event_output_direct(seq_handle, &out_ev);
                        }
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
