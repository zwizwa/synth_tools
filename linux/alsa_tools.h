#ifndef ALSA_TOOLS_H
#define ALSA_TOOLS_H

#include <alsa/asoundlib.h>

/* Docs say "negative error code".  Find out where to find them */
#define ALSA_ASSERT(cmd) \
    ({int err; if ((err=(cmd)) < 0) { \
            ERROR("%s: ALSA ERROR %d: %s\n", #cmd, err, snd_strerror(err)); }; err;})

static inline void alsa_connect(snd_seq_t *seq_handle,
                                snd_seq_addr_t sender, snd_seq_addr_t dest)
{
    // See alsa-utils/seq/aconnect/aconnect.c for example
    // Are these relevant?
    int queue = 0, convert_time = 0, convert_real = 0, exclusive = 0;

    snd_seq_port_subscribe_t *subs;
    snd_seq_port_subscribe_alloca(&subs);

    snd_seq_port_subscribe_set_sender(subs, &sender);
    snd_seq_port_subscribe_set_dest(subs, &dest);
    snd_seq_port_subscribe_set_queue(subs, queue);
    snd_seq_port_subscribe_set_exclusive(subs, exclusive);
    snd_seq_port_subscribe_set_time_update(subs, convert_time);
    snd_seq_port_subscribe_set_time_real(subs, convert_real);

    //ALSA_ASSERT(snd_seq_get_port_subscription(seq_handle, subs));
    ALSA_ASSERT(snd_seq_subscribe_port(seq_handle, subs));

}


#endif
