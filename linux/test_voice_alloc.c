// Use less voices to make logs simpler.
#define NB_VOICES 3
#define VOICE_MAX_SEMA 2

#include "mod_voice_alloc.c"

/* Use a global variable, that makes notation a little simpler. */
struct voice_alloc va;
void init() { voice_alloc_init(&va); }
void dump() { voice_alloc_dump(&va); }
void on(uint8_t note)  { voice_alloc_note_on(&va, note);  dump(); }
void off(uint8_t note) { voice_alloc_note_off(&va, note); dump(); }
void n_on (int nb, uint8_t note) { for(int i=0; i<nb; i++) { on(note);  } }
void n_off(int nb, uint8_t note) { for(int i=0; i<nb; i++) { off(note); } }

/* Invariants.

   For now it seems simpler to put the asserts in the code instead of
   building external asserts.  Printing after each op already is a
   good check.

*/



int main(int argc, char **argv) {
    init();
    dump();

    /* How to test this?  Emulate the scenarios that we design it to
       handle, then visually inspect the state.  Writing invariants is
       not something I can just do atm. */

    LOG("2 ON, 2 OFF\n");
    n_on(2, 64);
    n_off(2, 64);

    LOG("2 ON, 2 OFF\n");
    n_on(2, 64);
    n_off(2, 64);

    LOG("3 ON, 3 OFF\n");
    n_on(3, 64);
    n_off(3, 64);

    return 0;
}
