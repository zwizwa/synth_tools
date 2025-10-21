#include "macros.h"
#include <stdint.h>
#include <inttypes.h>

#include "osc.h"

/* TREE DEF */

void set_chan1(struct param_context *c, float f) {
    LOG("chan1 %f\n", f);
}
void set_chan2(struct param_context *c, float f) {
    LOG("chan2 %f\n", f);
}
void set_algo(struct param_context *c, int32_t i) {
    LOG("algo %d\n", i);
}

DEF_OSC_SET_FLOAT(chan1, "1", set_chan1);
DEF_OSC_SET_FLOAT(chan2, "2", set_chan2);
DEF_OSC_SET_INT(algo, "algo", set_algo);

DEF_OSC_LIST(chan, "chan", &chan1, &chan2);
DEF_OSC_LIST(root, "/", &chan, &algo);




/* TREE QUERY */




int test_osc(int argc, const char * const* argv) {
    ASSERT(argc == 3);
    const char *type = argv[1];
    ASSERT(type[0] == ',');
    ASSERT(type[2] == 0);
    char type_tag = type[1];
    const char *addr = argv[0];
    int addr_len = strlen(addr);            //LOG("addr_len = %d\n", addr_len);
    int msg_len = osc_len_scalar(addr_len); //LOG("msg_len = %d\n", msg_len);
    union osc msg[msg_len];
    int i = osc_make_scalar(&msg[0], msg_len,
                            addr, addr_len,
                            type_tag);
    switch(type_tag) {
    case 'f':
        msg[i].f = atof(argv[2]);
        break;
    case 'i':
        msg[i].i = atoi(argv[2]);
        break;
    default:
        ERROR("bad type_tag %c\n", type_tag);
    }

    /* Parse OSC message */
    struct param_context pc = {
        .root = root_list,
    };
    int e = osc_parse(&pc, msg, msg_len);
    LOG("e=%d, %s\n", e, osc_error(e));

    return e;
}

void test_scan(const char *str_ro, char sep) {
    LOG("scan: '%s'\n", str_ro);
    char *str = strdup(str_ro);
    struct scan scan;
    scan_init(&scan, str, sep);
    int count = 0;
    for(;;) {
        const char *el = scan_next(&scan);
        if (!el) break;
        LOG("scan:   %d el=%s\n", count, el);
        count++;
        if (count > 10) break;
    }
}

void log_path(struct osc_rev_path *path) {
    if (path) {
        LOG("/%s", path->name);
        log_path(path->parent);
    }
}

void test_traverse(struct param_context *pc,
                   struct osc_rev_path *path,
                   const struct param *param) {
    LOG("%s",param->name);
    log_path(path);
    LOG(":\n");
    LOG("%p %p\n",path,param);
}


void test(void) {

    /* Test the scanner */
    test_scan("/tag/2", '/');
    test_scan("/tag/2 123", ' ');
    test_scan("/tag/2   123", ' ');
    test_scan("/tag/2   123 ", ' ');
    test_scan(" /tag/2   123 ", ' ');
    test_scan("   /tag/2   123 ", ' ');
    test_scan("/a/*/b 567", '/');
 
    struct param_context pc = {
        .root = root_list,
    };
    (void)&root; // suppress unused static warning

    /* Create OSC float message. */
    if (1) {
        const char *argv[] = {"/chan/2", ",f", "123.456"};
        test_osc(3, argv);
    }

    if (1) {
        const char *argv[] = {"/algo", ",i", "123"};
        test_osc(3, argv);
    }

    if (1) {
        int e = osc_parse_text(&pc, "/chan/2 456.789");
        LOG("e=%d, %s\n", e, osc_error(e));
    }

    if (1) {
        int e = osc_parse_text(&pc, "/chan/2 456");
        LOG("e=%d, %s\n", e, osc_error(e));
    }

    if (1) {
        int e = osc_parse_text(&pc, "/algo 456.789");
        LOG("e=%d, %s\n", e, osc_error(e));

    }
    if (1) {
        int e = osc_parse_text(&pc, "/algo 123.456\n");
        LOG("e=%d, %s\n", e, osc_error(e));
    }
    if (1) {
        int e = osc_parse_text(&pc, "/chan/* 567\n");
        LOG("e=%d, %s\n", e, osc_error(e));
    }
    if (1) {
        osc_parse_addr_for(&pc, "/chan/*", "321");
    }

    osc_traverse(&pc, test_traverse);

}

int main(int argc, const char *const *argv) {
    if (argc > 1) { test_osc(argc-1, argv+1); }
    else test();
}
