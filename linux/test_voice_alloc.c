// Use less voices to make logs simpler.
#define NB_VOICES 8

#include "mod_voice_alloc.c"

int main(int argc, char **argv) {
    struct voice_alloc va;
    voice_alloc_init(&va);
    voice_alloc_dump(&va);

    /* How to test this?  Emulate the scenarios that we design it to handle. */

    return 0;
}
