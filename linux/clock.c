/* MIDI Clock code lifted from studio/c_src/jack_midi.c
   TODO: Add groove, sample sync. */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"

#include <jack/jack.h>
#include <jack/midiport.h>


/* JACK */
static jack_port_t *midi_out = NULL;
static jack_port_t *midi_in = NULL;
static jack_client_t *client = NULL;

// Send midi data out over a jack port.
static inline void send_midi(void *out_buf, jack_nframes_t time,
                             const void *data_buf, size_t nb_bytes) {
    //LOG("%d %d %d\n", frames, time, (int)nb_bytes);
    void *buf = jack_midi_event_reserve(out_buf, time, nb_bytes);
    if (buf) memcpy(buf, data_buf, nb_bytes);
}

#define BPM_TO_PERIOD(sr,bpm) ((sr*60)/(bpm*24))
static jack_nframes_t clock_time;
static jack_nframes_t bpm = 120;

static inline void process_clock_out(void *midi_out_buf, jack_nframes_t nframes, uint8_t stamp) {

    jack_nframes_t sr = jack_get_sample_rate(client);
    jack_nframes_t clock_period = BPM_TO_PERIOD(sr, bpm);

    /* Send out the MIDI clock bytes at the designated time slots */
    while(clock_time < nframes) {
        /* Clock pulse fits in current frame. */
        const uint8_t clock[] = {0xF8};
        send_midi(midi_out_buf, clock_time, clock, sizeof(clock));

        /* Advance clock */
        clock_time += clock_period;
    }
    /* Account for this frame */
    clock_time -= nframes;

}
static int process (jack_nframes_t nframes, void *arg) {
    /* Order is important. */

    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
    jack_midi_clear_buffer(midi_out_buf);
    jack_nframes_t f = jack_last_frame_time(client);
    uint8_t stamp = (f / nframes);
    process_clock_out(midi_out_buf, nframes, stamp);
    return 0;
}


int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "clock"; // argv[1];

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

    /* The main thread blocks on stdin.  The protocol is lowest common
       denominator.  While these are written to interface to Erlang,
       let's use midi as the main protocol so it is easier to reuse in
       different configurations. */
    for(;;) {
        // FIXME: Current function is just to exit when stdin is closed.
        uint8_t buf[1];
        assert_read(0, buf, sizeof(buf));
        // jack_client_close(client);
        exit(1);
    }
    return 0;
}

