#ifndef JACK_TOOLS_H
#define JACK_TOOLS_H

#include <jack/jack.h>
#include <jack/midiport.h>

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

#endif
