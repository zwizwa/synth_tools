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
#include <poll.h>
#include "jack_tools.h"
#include "mod_tether_3if_sysex.c"
#include "gdbstub_api.h"
#include "assert_write.h"
#include "tcp_tools.h"


/* 3IF */
struct tether_sysex s_ = {};
struct tether *s = &s_.tether;

/* TARGET READ CACHE */
#define CACHE_LOGSIZE 7 /* 7 fits in a single 3IF read command */
#define CACHE_SIZE (1 << CACHE_LOGSIZE)
#define CACHE_ADDR_MASK (~(CACHE_SIZE-1))
uint8_t cache_buf[CACHE_SIZE];
uint32_t cache_addr;

/* GDBSTUB */
#include "gdb/gdbstub.h"
const char gdbstub_memory_map[] = GDBSTUB_MEMORY_MAP_STM32F103CB;
struct gdbstub_config _config;
#include "gdb/rsp_packet.c"
#include "gdb/gdbstub.c"
GDBSTUB_INSTANCE(gdbstub, gdbstub_default_commands);
// All write access is stubbed out.
int32_t flash_erase(uint32_t addr, uint32_t size) {
    return 0;
}
int32_t flash_write(uint32_t addr, const uint8_t *b_buf, uint32_t len) {
    return 0;
}
int32_t mem_write(uint32_t addr, uint8_t val) {
    /* Not cached, so this is going to be slow. */
    if ((addr >= 0x20000000) &&
        (addr <  0x20005000)) {
        tether_write_mem(s, &val, addr, 1, LDA, NAS);
    }
    return E_OK;
}
int32_t mem_write32(uint32_t addr, uint32_t val) {
    return E_OK;
}
static inline int cached(uint32_t addr) {
    if (!cache_addr) return 0;
    return ((addr >= cache_addr) &&
            (addr < cache_addr + CACHE_SIZE));
}

void clear_cache(void) {
    if ((cache_addr >= 0x08000000) &&
        (cache_addr <  0x08020000)) {
        /* Flash is constant, so doesn't need this. */
        return;
    }
    cache_addr = 0;
}
void mem_prefetch(uint32_t addr) {
    if (cached(addr)) {
        // Already have this region
        return;
    }
    cache_addr = CACHE_ADDR_MASK & addr;
    if ((addr >= 0x20000000) &&
        (addr <  0x20005000)) {
        tether_read_mem(s, cache_buf, cache_addr, CACHE_SIZE, LDA, NAL);
        return;
    }
    if ((addr >= 0x08000000) &&
        (addr <  0x08020000)) {
        tether_read_mem(s, cache_buf, cache_addr, CACHE_SIZE, LDF, NFL);
        return;
    }
    clear_cache();
}
uint8_t mem_read(uint32_t addr) {
    mem_prefetch(addr);
    if (cached(addr)) {
        return cache_buf[addr - cache_addr];
    }
    else {
        LOG("bad addr 0x%08x\n", addr);
        return 0x55;
    }
}
void serve(int fd_in, int fd_out) {
    uint8_t buf[1024];
    for(;;) {
        ssize_t n_stdin = read(fd_in, buf, sizeof(buf)-1);
        if (n_stdin == 0) break;
        ASSERT(n_stdin > 0);
        buf[n_stdin] = 0;
        // LOG("I:%d:%s\n",n_stdin,buf);
        gdbstub_write(&gdbstub, buf, n_stdin);
        uint32_t n_stub = gdbstub_read_ready(&gdbstub);
        if (n_stub > sizeof(buf)-1) { n_stub = sizeof(buf)-1; }
        gdbstub_read(&gdbstub, buf, n_stub);
        buf[n_stub] = 0;
        // LOG("O:%d:%s\n", n_stub, buf);
        assert_write(fd_out, buf, n_stub);
        // FLUSH?
    }
}


/* JACK */
#define FOR_MIDI_IN(m)  m(midi_in)
#define FOR_MIDI_OUT(m) m(midi_out)
static jack_client_t *client = NULL;
FOR_MIDI_IN(DEF_JACK_PORT)
FOR_MIDI_OUT(DEF_JACK_PORT)
struct process_state {
    struct jack_pipes p; // base object
    void *midi_out_buf;
};
struct process_state process_state;
void send_midi_packet(struct jack_pipes *p,
                      const uint8_t *midi_data, uintptr_t nb_bytes) {
    // for(uintptr_t i=0; i<nb_bytes; i++) LOG(" %02x", midi_data[i]); LOG("\n");
    struct process_state *s = (void*)p;
    void *buf = jack_midi_event_reserve(s->midi_out_buf, 0, nb_bytes);
    if (buf) memcpy(buf, midi_data, nb_bytes);
}

static inline void *midi_out_buf(jack_port_t *port, jack_nframes_t nframes) {
    void *buf = jack_port_get_buffer(port, nframes);
    jack_midi_clear_buffer(buf);
    return buf;
}
static int process (jack_nframes_t nframes, void *arg) {
    process_state.midi_out_buf = midi_out_buf(midi_out, nframes);

    // FIXME: CLEAR!
    jack_pipes_handle(&process_state.p, send_midi_packet);
    FOR_MIDI_EVENTS(iter, midi_in, nframes) {
        const uint8_t *msg = iter.event.buffer;
        uintptr_t len = iter.event.size;
        // for(uintptr_t i=0; i<len; i++) LOG(" %02x", msg[i]); LOG("\n");
        assert_write(process_state.p.to_main_fd, msg, len);
    }
    return 0;
}


/* START & COMMAND DISPATCH */

typedef void (*push_fn)(struct tether *s, uint8_t byte);
/* Non-blocking mode: listen to both console input and midi port.  Not
   sure yet what to do with this. */
void push_cmd(struct tether *s, uint8_t byte) {
}
void push_midi(struct tether *s, uint8_t byte) {
}

int main(int argc, char **argv) {

    ASSERT(argc >= 3);
    const char *cmd = argv[2];

    const char *log_tag = getenv("TETHER_BL_TAG");
    if (log_tag) { tether_3if_tag = log_tag; }

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
        jack_pipes_init(&process_state.p);
        tether_set_midi_fds(
            &s_,
            /*fd_in*/  process_state.p.from_jack_fd,
            /*fd_out*/ process_state.p.to_jack_fd);
        jack_status_t status = 0;
        client = jack_client_open (client_name, JackNullOption, &status);
        ASSERT(client);
        FOR_MIDI_IN(REGISTER_JACK_MIDI_IN);
        FOR_MIDI_OUT(REGISTER_JACK_MIDI_OUT);
        jack_set_process_callback (client, process, 0);
        ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
        ASSERT(!jack_activate(client));
    }

    s->progress = 1;
    s->verbose = 1;

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

    if (!strcmp(cmd, "gdbstub")) {
        /* Serve gdbstub on stdio */
        // serve(0, 1);

        /* Serve gdbstub on TCP */
        uint16_t listen_port = 20000;
        int fd_listen = assert_tcp_listen(listen_port);
        int rv = 0;
        while (rv == 0) {
            int fd_con = assert_accept(fd_listen);
            LOG("accepted %d\n", listen_port);
            serve(fd_con, fd_con);
        }
    }


    if (!strcmp(cmd, "info")) {
        sleep(1);
        LOG("info\n");

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
