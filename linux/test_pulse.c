// TODO: Separate as mod_.c
// For now, hardcoded to cgen_synth=_out.h

/* PulseAudio wrapper. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pthread.h>

#include "cgen_synth_out.h"




#define ERROR(...) {fprintf (stderr, __VA_ARGS__); exit(1); }
#define LOG(...)   {fprintf (stderr, __VA_ARGS__); }


int main(int argc, char*argv[]) {
    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 1
    };

    LOG("channels = %d\n", ss.channels);

    pa_simple *s = NULL;
    int error;

    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, "pa_simple_new() failed: %d\n", error);
        goto exit;
    }

    struct synth_state state;

    for(;;) {  // FIXME: exit?
        struct synth_in in = { .i0 = {0.1, 0.2} };
        struct synth_out out;
        // int n = sizeof(out.o0) / sizeof(out.o0[0]);
        int n = 1024;
        uint16_t buf[n];

        // This produces a single mono output buffer of n samples.
        // State is updated or each frame.
        synth_update(&state, &in, &out);

        for (int i=0; i<n; i++) {
            // buf[i] = ((float)0x7FFF) * out.o0; //out.o0[i]; // FIXME
            buf[i] = i;
        }

        if (pa_simple_write(s, buf, (size_t) sizeof(buf), &error) < 0) goto exit;

    }
  exit:
    pa_simple_drain(s, &error); // ignore error
    if (s) pa_simple_free(s);
    return 0;
}
