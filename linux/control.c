/* Erlang MIDI hub.

   Handles all midi/Erlang routing.
   Hosts sequencer / arpeggiator.

   Clock is always slave mode here to keep things flexible.
   In my setup, clock.c is master clock.

   Note that this has all equipment hardcoded.  I currently do not see
   the point in adding a layer of configuration abstraction.  Easy
   enough to recompile in the current setup, so all config is in C, or
   C generated from compile-time config.  Later it might become
   obvious how to separate this out into config and generic code.

*/

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include "macros.h"
#include "jack_tools.h"
#include "assert_read.h"
#include "assert_write.h"
#include "tag_u32.h"

#include "mod_to_erl.c"


void send_tag_u32_buf_write(const uint8_t *buf, uint32_t len) {
    uint8_t len_buf[4];
    write_be(len_buf, len, 4);
    assert_write(1, len_buf, 4);
    assert_write(1, buf, len);
}
#define SEND_TAG_U32_BUF_WRITE send_tag_u32_buf_write
#include "mod_send_tag_u32.c"


static jack_client_t *client = NULL;


const char t_map[] = "map";
const char t_cmd[] = "cmd";

int reply_1(struct tag_u32 *req, uint32_t rv) {
    SEND_REPLY_TAG_U32(req, rv);
    return 0;
}
int reply_2(struct tag_u32 *req, uint32_t rv1, uint32_t rv2) {
    SEND_REPLY_TAG_U32(req, rv1, rv2);
    return 0;
}
int reply_ok(struct tag_u32 *req) {
    return reply_1(req, 0);
}
int reply_ok_1(struct tag_u32 *req, uint32_t val) {
    return reply_2(req, 0, val);
}
int reply_error(struct tag_u32 *req) {
    return reply_1(req, -1);
}


#define CMD_CONNECT 1
#define CMD_DISCONNECT 2

int handle_jack_port(struct tag_u32 *req) {
    TAG_U32_UNPACK(req, 0, m, cmd, nsrc, ndst) {
        if (!((m->nsrc > 0) &&
              (m->ndst > 0) &&
              (m->nsrc + m->ndst == req->nb_bytes))) {
            LOG("bad size: nsrc=%d, ndst=%d, bytes=%d\n",
                m->nsrc, m->ndst, req->nb_bytes);
            return reply_error(req);
        }
        char src[m->nsrc + 1];
        char dst[m->ndst + 1];
        memcpy(src, req->bytes, m->nsrc);
        memcpy(dst, req->bytes + m->nsrc, m->ndst);
        src[m->nsrc] = 0;
        dst[m->ndst] = 0;
        switch(m->cmd) {
        case CMD_CONNECT:
            jack_connect(client, src, dst);
            return reply_ok(req);
        case CMD_DISCONNECT:
            jack_disconnect(client, src, dst);
            return reply_ok(req);
        default:
            LOG("bad command %d\n", m->cmd);
            return reply_error(req);
        }
    }
    return -1;
}




/* Here "save" means from sequencer structure to tag_u32 return value,
   and "load" means tag_u32 argument to sequencer. */

int map_root(struct tag_u32 *req) {
    const struct tag_u32_entry map[] = {
        {"jack_port",     t_cmd, handle_jack_port, 3},
    };
    return HANDLE_TAG_U32_MAP(req, map);
}



int handle_tag_u32(struct tag_u32 *req) {
    int rv = map_root(req);
    if (rv) {
        LOG("handle_tag_u32 returned %d\n", rv);
        /* Always send a reply when there is a from address. */
        send_reply_tag_u32_status_cstring(req, 1, "bad_ref");
    }
    return 0;
}

/* Take over this functionality from jack_control.c
   I've added one level of {jack_control,...} wrapping to make Erlang code simpler. */
static void port_register(jack_port_id_t port_id, int reg, void *arg) {
    jack_port_t *port = jack_port_by_id(client, port_id);
    int flags = jack_port_flags(port);
    const char *port_name = jack_port_name(port);
    to_erl_ptermf("{jack_control,{port,%s,%s,\"%s\"}}",
                  reg ? "reg" : "unreg",
                  flags & JackPortIsInput ? "in" : "out",
                  port_name);
    char alias0[jack_port_name_size()];
    char alias1[jack_port_name_size()];
    char *const alias[2] = {alias0, alias1};
    int nb_alias = jack_port_get_aliases(port, alias);
    for (int i = 0; i<nb_alias; i++) {
        to_erl_ptermf("{jack_control,{alias,\"%s\",\"%s\"}}", port_name, alias[i]);
    }
    to_erl_flush();
}
static void port_connect(jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
    jack_port_t *pa = jack_port_by_id(client, a);
    jack_port_t *pb = jack_port_by_id(client, b);
    const char *na = jack_port_name(pa);
    const char *nb = jack_port_name(pb);
    to_erl_ptermf("{jack_control,{connect,%s,\"%s\",\"%s\"}}",
                  connect  ? "true" : "false", na, nb);
    to_erl_flush();

}
static void client_registration(const char *name, int reg, void *arg) {
    to_erl_ptermf("{jack,{client,%s,\"%s\"}}", reg ? "reg" : "unreg", name);
    to_erl_flush();
}

static int process (jack_nframes_t nframes, void *arg) {
    return 0;
}


int main(int argc, char **argv) {

    /* Jack client setup */
    const char *client_name = "control"; // argv[1];

    jack_status_t status = 0;
    client = jack_client_open (client_name, JackNullOption, &status);
    ASSERT(client);

    ASSERT(0 == jack_set_port_registration_callback(client, port_register, NULL));
    ASSERT(0 == jack_set_port_connect_callback(client, port_connect, NULL));
    ASSERT(0 == jack_set_client_registration_callback(client, client_registration, NULL));

    jack_set_process_callback (client, process, 0);
    ASSERT(!mlockall(MCL_CURRENT | MCL_FUTURE));
    ASSERT(!jack_activate(client));


    /* Use the generic {packet,4} + tag protocol on stdin, since hub.c
       might be hosting a lot of in-image functionality later. */
    for(;;) {
        uint8_t size_be[4];
        assert_read(0, size_be, 4);
        uint32_t size = read_be(size_be, 4);
        uint8_t buf[size];
        assert_read(0, buf, size);
        ASSERT(size >= 2);
        uint16_t tag = read_be(buf, 2);
        switch(tag) {
        case TAG_U32: {
            tag_u32_dispatch(handle_tag_u32,
                             send_reply_tag_u32,
                             NULL,
                             buf, size);
            break;
        }
        default:
            ERROR("unknown tag 0x%04x\n", tag);
        }
    }
    return 0;
}

