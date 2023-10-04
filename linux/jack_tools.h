#ifndef JACK_TOOLS_H
#define JACK_TOOLS_H

#define _GNU_SOURCE
#include <unistd.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include "macros.h"

#define DEF_JACK_PORT(name) \
    static jack_port_t *name = NULL;

#define REGISTER_JACK_MIDI_IN(name) \
    ASSERT(name = jack_port_register(client, #name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0));

#define REGISTER_JACK_MIDI_OUT(name) \
    ASSERT(name = jack_port_register(client, #name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0));

#define REGISTER_JACK_AUDIO_OUT(name) \
    ASSERT(name = jack_port_register(client, #name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));

/* MIDI iterator exposed as a FOR macro */
struct midi_cursor {
    void *buf;
    jack_midi_event_t event;
    jack_nframes_t i,n;
};
static inline __attribute__((always_inline))
struct midi_cursor midi_cursor_init(jack_port_t *port, jack_nframes_t nframes) {
    void *buf = jack_port_get_buffer(port, nframes);
    struct midi_cursor cur = {
        .buf = buf,
        .i = 0,
        .n = jack_midi_get_event_count(buf)
    };
    return cur;
}
static inline __attribute__((always_inline))
int midi_cursor_test(struct midi_cursor *cur) {
    int execute = cur->i < cur->n;
    if (execute) {
        jack_midi_event_get(&cur->event, cur->buf, cur->i);
    }
    return execute;
}
static inline __attribute__((always_inline))
void midi_cursor_update(struct midi_cursor *cur) {
    cur->i++;
}
#define FOR_MIDI_EVENTS(cur, port, nframes) \
    for(struct midi_cursor cur = midi_cursor_init(port, nframes); \
        midi_cursor_test(&cur); \
        midi_cursor_update(&cur))


/* Some default is necessary for apps that have a main thread and a
   jack thread.  To keep it simple, standarize on a pipe each way.
   This uses pipe2. */

#ifdef _GNU_SOURCE
struct jack_pipes {
    int from_main_fd, to_main_fd;
    int from_jack_fd, to_jack_fd;
};
static inline
void jack_pipes_init(struct jack_pipes *p) {
    /* To jack is in packet mode. */
    int main_to_jack[2]; ASSERT(0 == pipe2(main_to_jack, O_DIRECT | O_NONBLOCK));
    p->from_main_fd = main_to_jack[0];
    p->to_jack_fd   = main_to_jack[1];
    /* Return is NOT in packet mode.  FIXME: This feels ugly, but
       that's what the tether_3if code wants.  Maybe add buffering. */
    int jack_to_main[2]; ASSERT(0 == pipe2(jack_to_main, 0));
    p->from_jack_fd = jack_to_main[0];
    p->to_main_fd   = jack_to_main[1];
}
void jack_pipes_handle(struct jack_pipes *p,
                       void (*handle)(struct jack_pipes *,
                                      const uint8_t *, uintptr_t)) {
    int rv;
    uint8_t buf[1024]; // What is a good size?
    /* We only expect two conditions: non-zero size packet and no
       data.  Anything else is an error. */
    while ((rv = read(p->from_main_fd, buf, sizeof(buf))) > 0) {
        handle(p, buf, rv);
    }
    ASSERT(rv == -1);
    ASSERT(errno == EAGAIN);
}
#else
#error jack_pipes needs _GNU_SOURCE for pipe2()
#endif


#endif
