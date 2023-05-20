#include "macros.h"
#include <stdint.h>

/* This we need to stub out, as it refers to STM memory.  Code below
   is cloned from mod_monitor.c */

#define MONITOR_3IF_LOG(s, ...) LOG(__VA_ARGS__)
#include "mod_monitor_3if.c"
#define FLASH_BUFSIZE_LOG 1
#define FLASH_BUFSIZE (1 << FLASH_BUFSIZE_LOG)
struct monitor {
    struct monitor_3if monitor_3if;
    uint8_t out_buf[256 + 16];
    struct cbuf out;
    uint8_t ds_buf[32];
    uint8_t flash_buf[FLASH_BUFSIZE];
    uint8_t last_read_was_full:1;
};
struct monitor monitor;
void monitor_write(const uint8_t *buf, uintptr_t size) {
    for (uintptr_t i=0; i<size; i++) {
        LOG("M: %02x\n", buf[i]);
    }
    int rv = monitor_3if_write(&monitor.monitor_3if, buf, size);
    (void)rv;
}
void monitor_init(void) {
    CBUF_INIT(monitor.out);
    monitor_3if_init(
        &monitor.monitor_3if,
        &monitor.out,
        monitor.ds_buf);
}


/* The MIDI bootloader protocol is on top of that. */
#define BL_MIDI_LOG(s, ...) LOG(__VA_ARGS__)
#define MOD_MONITOR
#include "mod_bl_midi.c"

void test(void) {
    const uint8_t midi[] = {
        0xf0, 0x12, 0x0e, 0x03, 0x49, 0x4a, 0x4b, 0xf7
    };
    bl_midi_write_sysex(&bl_state, midi, sizeof(midi));
}

int main(int argc, char **argv) {
    monitor_init();
    test();
    return 0;
}
