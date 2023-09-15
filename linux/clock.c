/* MIDI Clock code lifted from studio/c_src/jack_midi.c
   TODO: Add groove, sample sync. */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "assert_read.h"

#include <jack/jack.h>
#include <jack/midiport.h>


/* JACK */
static jack_port_t *audio_out = NULL;
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

#define BPM_TO_HPERIOD(sr,bpm) ((sr*30)/(bpm*24))
static jack_nframes_t bpm = 120;
static float clock_phase = 0.0f;
static int clock_pol = 1;

static int process (jack_nframes_t nframes, void *arg) {

    jack_nframes_t sr = jack_get_sample_rate(client);
    float clock_hperiod = BPM_TO_HPERIOD(sr, bpm);

    /* Generate quare wave output, send midi on positive edge. */
    jack_default_audio_sample_t *audio_out_buf = jack_port_get_buffer(audio_out, nframes);
    void *midi_out_buf = jack_port_get_buffer(midi_out, nframes);
    jack_midi_clear_buffer(midi_out_buf);
    for (int t=0; t<nframes; t++) {
        if (clock_phase >= clock_hperiod) {
            clock_phase -= clock_hperiod;
            clock_pol ^= 1;
            if (clock_pol == 1) {
                const uint8_t clock[] = {0xF8};
                send_midi(midi_out_buf, t, clock, sizeof(clock));
            }
        }
        audio_out_buf[t] = clock_pol;
        clock_phase += 1;
    }
    return 0;
}


int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "clock"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    ASSERT(midi_in = jack_port_register(
               client, "control",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));

    ASSERT(midi_out = jack_port_register(
               client, "midi",
               JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0));

    ASSERT(audio_out = jack_port_register(
               client, "wave",
               JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));

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

