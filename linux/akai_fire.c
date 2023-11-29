// MIDI Clock code lifted from studio/c_src/jack_midi.c

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "mod_akai_fire.c"

#include "macros.h"
#include "assert_read.h"
#include "assert_write.h"


#include "midi_frame.h"

struct akai_fire _fire = {
};


/* JACK */
static jack_port_t *midi_out = NULL;
static jack_port_t *midi_in = NULL;
static jack_client_t *client = NULL;

#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))


static int process(jack_nframes_t nframes, void *arg) {
    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
    jack_midi_clear_buffer(midi_out_buf);
    void *midi_in_buf = jack_port_get_buffer(midi_in, nframes);
    akai_fire_process(&_fire, midi_out_buf, midi_in_buf);
    return 0;
}


void fire_start(struct akai_fire *fire) {
    fire->need_update = 1;

    /* Jack client setup */
    const char *client_name = "jack_akai_fire"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    ASSERT(midi_in = jack_port_register(
               client, "in",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));

    ASSERT(midi_out = jack_port_register(
               client, "out",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0));

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));

    /* The main thread blocks on stdin.  The protocol should be lowest
       common denominator.  While these are written to interface to
       Erlang, let's use midi as the main protocol so it is easier to
       reuse in different configurations. */

    /* However, that is too much work for what I need to do now, so
       use framing. */

    /* Send something to signal the other end we're all set, i.e. all
       jack ports are available.  The Active Sensing message is ideal
       for this. */

#if 1
    ASSERT_WRITE(1, 0,0,0,2, 0xff,0xfc); // ping
    for(;;) {
        uint8_t buf[1024];
        ssize_t nb_read = read(0, buf, sizeof(buf));
        (void)nb_read;
        exit(1);
    }
#else
    ASSERT_WRITE(1, 0xFE);
    struct midi_frame f;
    midi_frame_init(&f, midi);
    for(;;) {
        uint8_t buf[1024];
        ssize_t nb_read = read(0, buf, sizeof(buf));
        ASSERT(nb_read > 0);
        midi_frame_push_buf(&f, buf, nb_read);
    }
#endif
}

int main(int argc, char **argv) {
    fire_start(&_fire);
    return 0;
}

