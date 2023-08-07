#ifndef MOD_PATTERN
#define MOD_PATTERN

#include <stdint.h>

#define PATTERN_SIZE 16

struct pattern_event {
    uint8_t velocity;   // Includes accent
    uint8_t instrument;
};
struct pattern {
    struct pattern_event pattern[PATTERN_SIZE];
};


#endif

