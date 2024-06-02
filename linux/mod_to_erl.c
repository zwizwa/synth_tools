#ifndef MOD_TO_ERL
#define MOD_TO_ERL

#include "assert_write.h"
#include "erl_port.h"
#include <stdarg.h>



/* Erlang */
#define TO_ERL_SIZE_LOG 16

#define TO_ERL_SIZE (1 << TO_ERL_SIZE_LOG)
static uint8_t to_erl_buf[TO_ERL_SIZE];
static size_t to_erl_buf_bytes = 0;
//static uint32_t to_erl_room(void) {
//    uint32_t free_bytes = sizeof(to_erl_buf) - to_erl_buf_bytes;
//    if (free_bytes >= 6) return free_bytes - 6;
//    return 0;
//}
static inline uint8_t *to_erl_hole_8(int nb, uint16_t port) {
    size_t msg_size = 8 + nb;
    if (to_erl_buf_bytes + msg_size > sizeof(to_erl_buf)) {
        LOG("erl buffer overflow\n");
        return NULL;
    }
    uint8_t *msg = &to_erl_buf[to_erl_buf_bytes];
    /* Midi is mapped into generic stream tag.  Maybe this should have
       its own tag?  We do need to guarantee single midi messages
       here. */
    set_u32be(msg, msg_size - 4); // {packet,4}
    set_u16be(msg+4, 0xFFFB); // TAG_STREAM
    set_u16be(msg+6, port);
    to_erl_buf_bytes += msg_size;
    return &msg[8];
}
static uint8_t *to_erl_hole_6(int nb) {
    size_t msg_size = 6 + nb;
    if (to_erl_buf_bytes + msg_size > sizeof(to_erl_buf)) {
        LOG("erl buffer overflow\n");
        return NULL;
    }
    uint8_t *msg = &to_erl_buf[to_erl_buf_bytes];
    set_u32be(msg, msg_size - 4); // {packet,4}
    set_u16be(msg+4, 0xFFEE); // TAG_PTERM
    to_erl_buf_bytes += msg_size;
    return &msg[6];
}
void to_erl_pterm(const char *pterm) {
    int nb = strlen(pterm);
    uint8_t *hole = to_erl_hole_6(nb);
    if (hole) {
        // LOG("sending pterm %s\n", pterm);
        memcpy(hole, pterm, nb);
    }
    else {
        LOG("WARNING: not sending pterm %s\n", pterm);
    }
}
static void to_erl_ptermvf(const char *fmt, va_list ap) {
    char *pterm = NULL;
    ASSERT(-1 != vasprintf(&pterm, fmt, ap));
    to_erl_pterm(pterm);
}
// FIXME: This is so common it deserves a macro in uc_tools
static void to_erl_ptermf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    to_erl_ptermvf(fmt, ap);
    va_end(ap);
}


void to_erl_midi(const uint8_t *buf, int nb, uint8_t port) {
    uint8_t *hole = to_erl_hole_8(nb, port);
    if (hole) { memcpy(hole, buf, nb); }
}

static inline void to_erl_flush(void) {
    if (to_erl_buf_bytes) {
        //LOG("buf_bytes = %d\n", (int)to_erl_buf_bytes);
        // FIXED?
        assert_write(1, to_erl_buf, to_erl_buf_bytes);
        to_erl_buf_bytes = 0;
    }
}


#endif
