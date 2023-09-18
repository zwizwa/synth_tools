#ifndef JACK_MACROS_H
#define JACK_MACROS_H

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

#endif
