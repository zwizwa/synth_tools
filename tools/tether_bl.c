/* The synth_tools default is to use the 3if as the main "user
   interface", meaning the applications will never switch protocol,
   and all application-level message passing is handled by a 3if
   intermediate. */

#include "mod_tether_3if.c"
#include "uct_byteswap.h"

void handle_async(struct tether *s) {
}

int main(int argc, char **argv) {

    const char *log_tag = getenv("TETHER_BL_TAG");
    if (log_tag) { tether_3if_tag = log_tag; }

    ASSERT(argc >= 3);
    const char *dev = argv[1];
    const char *cmd = argv[2];

    struct tether s;
    tether_open_tty(&s, dev);
    s.verbose = 0;
    s.progress = isatty(0);

    // ./tether_bl.dynamic.host.elf /dev/ttyACM0 load 0x08004000 ../stm32f103/synth.x8ab.f103.fw.bin

    if (!strcmp(cmd,"load")) {
        ASSERT(argc == 5);
        uint32_t address = strtol(argv[3], NULL, 0);
        const char *binfile = argv[4];
        LOG("%s%08x load %s\n", tether_3if_tag, address, binfile);
        tether_load_flash(&s, binfile, address);
        return 0;
    }

    /* Send an application packet to start the app. */
    if (!strcmp(cmd,"start")) {
        uint8_t msg[] = {
            U32_BE(4),
            /* See uc_tools/packet_tags.h This is a generic event tag
               that does not expect a reply. */
            U16_BE(0xFFF3),
            U16_BE(0),
        };
        assert_write(s.fd, msg, sizeof(msg));
        return 0;
    }

    /* See mod_monitor_3if.c for more information about async
       operation.  What we need to know here is: 1) alwys run "flush"
       before interacting witht he bootloader.  This will disable
       async poll and will remove all async messages from the
       queue. */

    /* Wait for event. */
    if (!strcmp(cmd,"wait")) {
        tether_read(&s);
        handle_async(&s);
        return 0;
    }

    /* Flush async messages. */
    if (!strcmp(cmd,"flush")) {
        tether_flush(&s, handle_async);
        return 0;
    }

    LOG("unknown cmd '%s'\n", cmd);
    return 1;
}

