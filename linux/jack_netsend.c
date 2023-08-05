// Playground for midi + audio synth.
// Midi part is cloned from jack_midi.c


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "macros.h"

#include "sysex.h"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

#include "tcp_tools.h"
#include "assert_write.h"



#include <stdio.h>
#include <math.h>
#include <string.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)


/* TCP */
int sock_fd = -1;



/* JACK */

static int nb_midi_in;   static jack_port_t **midi_in   = NULL;

static jack_client_t *client = NULL;

static inline void process_midi(jack_nframes_t nframes) {
    char buf[1024];

    for (int in=0; in<nb_midi_in; in++) {
        void *midi_in_buf  = jack_port_get_buffer(midi_in[in], nframes);
        jack_nframes_t n = jack_midi_get_event_count(midi_in_buf);
        for (jack_nframes_t i = 0; i < n; i++) {

            jack_midi_event_t event;
            jack_midi_event_get(&event, midi_in_buf, i);
            const uint8_t *msg = event.buffer;

            if (in == 0 &&
                event.size == 3 &&
                msg[0] == 0xB0) { // CC channel 0
                sprintf(buf, "cc %d %d;\n", msg[1], msg[2]);
            }
            else if (in == 0 &&
                event.size == 3 &&
                msg[0] == 0x90) { // Note on channel 0
                sprintf(buf, "on %d %d;\n", msg[1], msg[2]);
            }
            else if (in == 0 &&
                event.size == 3 &&
                msg[0] == 0x80) { // Note off channel 0
                sprintf(buf, "off %d %d;\n", msg[1], msg[2]);
            }

            assert_write(sock_fd, (uint8_t*)buf, strlen(buf));
            LOG("%s", buf);

        }
    }
}

static int process (jack_nframes_t nframes, void *arg) {
    /* Order is important. */
    process_midi(nframes);
    return 0;
}


int main(int argc, char **argv) {

    if (argc != 3) {
        LOG("usage: %s <host> <port>\n", argv[0]);
        return -1;
    }

    /* TCP */
    sock_fd = assert_tcp_connect(argv[1], atoi(argv[2]));


    /* Jack client setup */
    const char *client_name = "jack_netsend"; // argv[1];
    nb_midi_in = 1;
    midi_in = calloc(nb_midi_in,   sizeof(void*));

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    char port_name[32] = {};
    for (int in = 0; in < nb_midi_in; in++) {
        snprintf(port_name,sizeof(port_name)-1,"midi_in_%d",in);
        LOG("i: %s\n", port_name);
        ASSERT(midi_in[in] = jack_port_register(
                   client, port_name,
                   JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));
    }
    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* Input loop. */
    for(;;) {
        /* Not processing stdin.  Later: use rai.erl {packet,4}
           TAG_U32 protocol. */
        sleep(1);
    }
    return 0;
}

