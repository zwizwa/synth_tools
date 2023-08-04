/* The fd is MIDI.  The 3if commands are tunneled over sysex. */

#include "mod_tether_3if_sysex.c"
#include "gdbstub_api.h"
#include <poll.h>

typedef void (*push_fn)(struct tether *s, uint8_t byte);

/* Non-blocking mode: listen to both console input and midi port.  Not
   sure yet what to do with this. */
void push_cmd(struct tether *s, uint8_t byte) {
}
void push_midi(struct tether *s, uint8_t byte) {
}

/* Application defines 3if command extensions. */


int main(int argc, char **argv) {

    ASSERT(argc >= 3);
    const char *dev = argv[1];
    const char *cmd = argv[2];

    const char *log_tag = getenv("TETHER_BL_TAG");
    if (log_tag) { tether_3if_tag = log_tag; }

    struct tether_sysex s_ = {};
    tether_open_midi(&s_, dev);

    struct tether *s = &s_.tether;
    s->progress = 1;
    s->verbose = 0;

    if (!strcmp(cmd,"load")) {
        ASSERT(argc == 5);
        uint32_t address = strtol(argv[3], NULL, 0);
        const char *binfile = argv[4];
        LOG("%s%08x load %s\n", tether_3if_tag, address, binfile);
        tether_load_flash(s, binfile, address);
        return 0;
    }

    if (!strcmp(cmd,"midi")) {
        uint32_t n = argc - 3;
        uint8_t midi[n];
        for (uint32_t i=0; i<n; i++) {
            midi[i] = strtol(argv[3 + i], NULL, 0);
            LOG(" %02x", midi[i]);
        }
        LOG("\n");
        assert_write(s->fd, midi, n);
        return 0;
    }

    if (!strcmp(cmd, "service")) {
        /* Not finsihed yet.
           What should this look like?  I want:
           - log display
           - command entry
           Maybe the simplest thing to do really is a GDBstub interface.
         */
        int timeout_ms = 1000;
        int cmd_fd = 0;
        struct pollfd pfd[] = {
            {.fd = cmd_fd, .events = POLLIN},
            {.fd = s->fd,  .events = POLLIN},
        };
        push_fn push[] = {
            push_cmd,
            push_midi,
        };

        int nb_ports = ARRAY_SIZE(pfd);
        for (;;) {
            int rv;
            ASSERT_ERRNO(rv = poll(&pfd[0], nb_ports, timeout_ms));
            for (int i=0; i<nb_ports; i++) {
                if( pfd[i].revents & POLLIN) {
                    uint8_t buf[1024];
                    size_t size = assert_read(pfd[i].fd, buf, sizeof(buf));
                    for (size_t i=0; i<size; i++) {
                        (push[i])(s, buf[i]);
                    }
                }
            }
        }
    }

    if (!strcmp(cmd, "info")) {
        /* Address of the list if 3if command extensions. */
        uint32_t cmd_3if = tether_read_flash_u32(
            s, 0x08002800 + 4 * GDBSTUB_CONFIG_INDEX_CMD_3IF);
        // LOG("%08x cmd_3if\n", cmd_3if);
        /* The first one in synth is the log poll. */
        uint32_t cmd_3if_info = tether_read_flash_u32(s, cmd_3if);
        // LOG("%08x cmd_3if_info\n", cmd_3if_info);

        tether_intr(s, cmd_3if_info);
        if (s->buf[0] > 1) {
            assert_write(2, &s->buf[2], s->buf[0] - 1);
        }

        return 0;
    }


    LOG("unknown cmd '%s'\n", cmd);
    return 1;

}
