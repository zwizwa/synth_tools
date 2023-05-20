
#include "mod_tether_3if_sysex.c"

int main(int argc, char **argv) {

    ASSERT(argc >= 3);
    const char *dev = argv[1];
    const char *cmd = argv[2];

    const char *log_tag = getenv("TETHER_BL_TAG");
    if (log_tag) { tether_3if_tag = log_tag; }

    struct tether_sysex s_ = {};
    tether_open_midi(&s_, dev);

    struct tether *s = &s_.tether;

    if (!strcmp(cmd,"load")) {
        ASSERT(argc == 5);
        uint32_t address = strtol(argv[3], NULL, 0);
        const char *binfile = argv[4];
        LOG("%s%08x load %s\n", tether_3if_tag, address, binfile);
        tether_load_flash(s, binfile, address);
        return 0;
    }

    LOG("unknown cmd '%s'\n", cmd);
    return 1;

    return 0;
}
