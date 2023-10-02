/* The fd is MIDI.  The 3if commands are tunneled over sysex.

   Some more context.

   - The serial bootloader uses {packet,8} which is probably a
     mistake.  Switching protocols is hard.  Switching them back and
     forth is harder.  So this one is always the same: it uses MIDI
     SYSEX, and only the hooks change.

   - While BOOT1=1 is still supported for debricking (i.e. don't start
     the application), the default operation mode is to start the
     application and be able to stop it such that a new firmware image
     can be loaded and started.

   - I want to use GDB as a console.  Provide a mode here that starts
     a gdbstub on top of 3if.

*/

/* Jack Design

   Always a 2-thread design.  Since we can use a blocking interface in
   the main thread, the MIDI return path can be a pipe.  The MIDI
   command path can be a circular buffer.

*/

// #define TETHER_3IF_LOG_DBG LOG

#define _GNU_SOURCE // Needed for pipe2 in jack_tools.h
#include "jack_tools.h"
#include "mod_tether_3if_sysex.c"
#include "gdbstub_api.h"
#include <poll.h>

/* JACK */
#define FOR_MIDI_IN(m)  m(midi_in)
#define FOR_MIDI_OUT(m) m(midi_out)
static jack_client_t *client = NULL;
FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)
static struct jack_pipes jack_pipes;
static int process (jack_nframes_t nframes, void *arg) {
    /* Perform a non-blocking read on the pipe.  The pipe contains
       stream data that we convert to midi messages here. */
    int timeout_ms = 0;
    struct pollfd pfd = {
        .fd = jack_pipes.from_main_fd, .events = POLLIN,
    };
    int rv;
    ASSERT_ERRNO(rv = poll(&pfd, 1, timeout_ms));
    if( pfd.revents & POLLIN) {
        uint8_t buf[1024];
        size_t size = assert_read(pfd.fd, buf, sizeof(buf));
        for (size_t i=0; i<size; i++) {
            LOG("%d 0x%02\n", i, buf[i]);
        }
    }
    return 0;
}


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
    const char *cmd = argv[2];

    const char *log_tag = getenv("TETHER_BL_TAG");
    if (log_tag) { tether_3if_tag = log_tag; }

    struct tether_sysex s_ = {};

    ASSERT(argv[1][0]);
    if (argv[1][0] == '/') {
        const char *dev = argv[1];
        // FIXME: Make this more clear later.  If the device starts
        // with a slash it is assumed to be an old style midi device.
        tether_open_midi(&s_, dev);
    }
    else {
        // Otherwise it is a jack client name.
        const char *client_name = argv[1];
        jack_pipes_init(&jack_pipes);
        tether_set_midi_fds(
            &s_,
            /*fd_in*/  jack_pipes.from_jack_fd,
            /*fd_out*/ jack_pipes.to_jack_fd);
        jack_status_t status = 0;
        client = jack_client_open (client_name, JackNullOption, &status);
        ASSERT(client);
        FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
        FOR_MIDI_OUT(REGISTER_JACK_MIDI_OUT);
        jack_set_process_callback (client, process, 0);
        ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
        ASSERT(!jack_activate(client));
    }

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
        assert_write(s->fd_out, midi, n);
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
            {.fd = cmd_fd,   .events = POLLIN},
            {.fd = s->fd_in, .events = POLLIN},
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
            assert_write(1, &s->buf[2], s->buf[0] - 1);
        }

        return 0;
    }

    LOG("unknown cmd '%s'\n", cmd);
    return 1;

}
