// Playground for midi + audio synth.
// Midi part is cloned from jack_midi.c


#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "macros.h"

#include "sysex.h"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>

static jack_client_t *client = NULL;

void log_ports(void) {
    const char **ports = jack_get_ports(client, NULL, NULL, 0);
    for (int i=0; ports[i]; i++) {
        LOG("%d %s\n", i, ports[i]);
    }
    jack_free(ports);
}

/* These callbacks update the database of clients and ports.  It seems
   simplest to call jack_get_ports() on every change. */
void client_reg_cb(const char *name, int iarg, void *arg) {
    LOG("clientreg_cb %s %s\n", name, iarg ? "con" : "dis");
}
void port_reg_cb(jack_port_id_t port_id, int iarg, void *varg) {
    const jack_port_t *port = jack_port_by_id(client, port_id);
    const char *name = jack_port_name(port);
    LOG("port_reg_cb %s %s\n", name, iarg  ? "con" : "dis");
}

int main(int argc, char **argv) {
    const char *client_name = "jack_info";
    jack_status_t status = 0;
    ASSERT(client = jack_client_open (client_name, JackNullOption, &status));

    log_ports();

    jack_set_port_registration_callback(client, port_reg_cb, NULL);
    jack_set_client_registration_callback(client, client_reg_cb, NULL);

    jack_activate(client);

    /* Input loop. */
    for(;;) {
        /* Not processing stdin. */
        sleep(1);
    }

    return 0;
}

