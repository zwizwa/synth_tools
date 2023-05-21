#include "macros.h"
#include <stdint.h>

/* This we need to stub out, as it refers to STM memory.  Code below
   is cloned from mod_monitor.c */

#define MONITOR_3IF_LOG(s, ...) LOG(__VA_ARGS__)
#include "mod_monitor_3if.c"
struct monitor {
    struct monitor_3if monitor_3if;
    uint8_t out_buf[256 + 16];
    struct cbuf out;
    uint8_t ds_buf[32];
};
struct monitor monitor;
void monitor_write(const uint8_t *buf, uintptr_t size) {
    for (uintptr_t i=0; i<size; i++) {
        LOG("M: %02x\n", buf[i]);
    }
    ASSERT(0 == monitor_3if_write(&monitor.monitor_3if, buf, size));
}
void monitor_init(void) {
    CBUF_INIT(monitor.out);
    monitor_3if_init(
        &monitor.monitor_3if,
        &monitor.out,
        monitor.ds_buf);
}


#include "gdbstub_api.h"
void start(void) {
    LOG("start()\n");
}
void midi_write(const uint8_t *buf, uint32_t len) {
}
uint32_t midi_read(uint8_t *buf, uint32_t room) {
    return 0;
}
const struct gdbstub_io midi_io = {
    .read  = midi_read,
    .write = midi_write,
};

struct gdbstub_config _config = {
    .start = start,
    .io = &midi_io,
};

/* The MIDI bootloader protocol is on top of that. */
#define BL_MIDI_LOG(s, ...) LOG(__VA_ARGS__)
#define MOD_MONITOR
#include "mod_bl_midi.c"


void test(void) {

    /* Go through a full sequence. */

    /* 1. host command. */
    const uint8_t cmd[] = {
        4, NPUSH, 1,2,3,
    };
    /* 2. encode as chunked UMP */
    struct cbuf c_cmd; uint8_t c_cmd_buf[256];
    CBUF_INIT(c_cmd);
    cbuf_write(&c_cmd, cmd, sizeof(cmd));

    uint8_t cmd_ump[256];
    slice_uint8_t cmd_ump_slice = { .buf = cmd_ump, .len = sizeof(cmd_ump) };
    sysex_stream_from_cbuf(0x12, &cmd_ump_slice, &c_cmd);
    uint32_t nb_ump = cmd_ump_slice.buf - cmd_ump;
    LOG("nb_ump = %d\n", nb_ump);
    ASSERT(12 == nb_ump);

    /* 3. Pass it into the read routine.  This will push it all the
       way through the interpreter, leaving a reply in the out
       buffer. */
    bl_midi_write(&bl_state, cmd_ump, nb_ump);

    uint32_t nb_3if_out = cbuf_elements(&monitor.out);
    LOG("nb_3if_out = %d\n", nb_3if_out);
    ASSERT(2 == nb_3if_out); // 01 00

    /* 4. Convert the reply to UMP */
    uint8_t usb_out[64];
    uint32_t nb_3if_out_ump = monitor_read_sysex(usb_out, sizeof(usb_out));
    LOG("nb_3if_out_ump = %d\n", nb_3if_out_ump);


    ASSERT(8 == nb_3if_out_ump);

    // FIXME: Parse it again, plug in the tether routines.

#if 0
    uint8_t midi[] = {
        0xf0, 0x12,
        // Note that 3if can be padded with zeros = 0 size packets.
        0,0,0,0,0,0,
        0xf7
    };
    const_slice_uint8_t in = { .buf = cmd, .len = sizeof(cmd) };

    uintptr_t nb = sysex_encode_8bit_to_7bit(&midi[2], &in);
    ASSERT(6 == nb);

    bl_midi_write_sysex(&bl_state, midi, sizeof(midi));
#endif

#if 0
    LOG("out %d\n", cbuf_elements(&monitor.out));
    uint8_t reply[8 * 3];
    slice_uint8_t reply_slice = { .buf = reply, .len = sizeof(reply) };
    sysex_stream_from_cbuf(0x12, &reply_slice, &monitor.out);
    uint32_t nb_reply = reply_slice.buf - reply;
    LOG("nb_reply %d\n", nb_reply);
#endif

}

/* The sysex.h subroutines */
void assert_sysex(void) {
    ASSERT(0 == sysex_encode_8bit_to_7bit_payload_available(0));
    ASSERT(0 == sysex_encode_8bit_to_7bit_payload_available(1));
    ASSERT(1 == sysex_encode_8bit_to_7bit_payload_available(2));
    ASSERT(2 == sysex_encode_8bit_to_7bit_payload_available(3));
    ASSERT(7 == sysex_encode_8bit_to_7bit_payload_available(8));
    ASSERT(7 == sysex_encode_8bit_to_7bit_payload_available(9));
    ASSERT(8 == sysex_encode_8bit_to_7bit_payload_available(10));
}

void test_stream_from_cbuf(const uint8_t *in_buf, uint32_t in_len) {
    LOG("i:");
    for(uint32_t i=0; i<in_len; i++) {
        LOG(" %02x", in_buf[i]);
    }
    LOG("\n");
    struct cbuf c; uint8_t c_buf[256];
    CBUF_INIT(c);
    cbuf_write(&c, in_buf, in_len);
    uint8_t out_buf[64] = {};
    slice_uint8_t out_slice = { .buf = out_buf, .len = sizeof(out_buf) };
    sysex_stream_from_cbuf(0x12, &out_slice, &c);
    uint32_t out_nb = out_slice.buf - out_buf;

    uint32_t out_chunks = out_nb / 4;
    for(uint32_t i=0; i<out_chunks; i++) {
        LOG("o:");
        for(uint32_t j=0; j<4; j++) {
            LOG(" %02x", out_buf[j + 4 * i]);
        }
        LOG("\n");
    }
}
#define TEST_STREAM_FROM_CBUF(...) {                    \
        const uint8_t msg[] = { __VA_ARGS__ };          \
        test_stream_from_cbuf(msg, sizeof(msg));        \
    }

void test_stream_from_cbufs(void) {
    TEST_STREAM_FROM_CBUF(4, NPUSH, 1, 2, 3);
    TEST_STREAM_FROM_CBUF(3, 201, 202, 203);
    TEST_STREAM_FROM_CBUF(2, 201, 202);
}

int main(int argc, char **argv) {
    assert_sysex();
    test_stream_from_cbufs();

    monitor_init();
    test();
    test();
    test();
    return 0;
}
