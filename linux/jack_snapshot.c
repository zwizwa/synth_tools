// Adapted from https://github.com/jackaudio/tools/blob/master/lsp.c
// Original copyright notice:

/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>

#include <jack/jack.h>
#include <jack/session.h>
#include <jack/uuid.h>

#include "macros.h"

char * my_name;

static void print_port(const char *str) {
    int sub_colon = 1;
    while (*str != 0) {
        if (sub_colon && *str == ':') {
            putchar(',');
            sub_colon = 0; // only first one. a2jmidid ports contain colon
        }
        else {
            putchar(*str);
        }
        str++;
    }
}
static void print_connection(const char *src, const char *dst) {
    print_port(src);
    printf(",");
    print_port(dst);
    printf("\n");
}

int main (int argc, char *argv[]) {
    jack_client_t *client;
    jack_status_t status;
    jack_options_t options = JackNoStartServer;
    const char **ports, **connections;
    char *server_name = NULL;

    my_name = strrchr(argv[0], '/');
    if (my_name == 0) {
        my_name = argv[0];
    } else {
        my_name ++;
    }

    /* Open a client connection to the JACK server.  Starting a new
     * server only to list its ports seems pointless, so we specify
     * JackNoStartServer. */
    if ((client = jack_client_open ("jack_snapshot", options, &status, server_name)) == 0) {
        fprintf (stderr, "Error: cannot connect to JACK, ");
        if (status & JackServerFailed) {
            fprintf (stderr, "server is not running.\n");
        } else {
            fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
        }
        return 1;
    }

    ports = jack_get_ports (client, NULL, NULL, 0);


    for (int i = 0; ports && ports[i]; ++i) {

        /* Print out -> in only. */
        jack_port_t *port = jack_port_by_name (client, ports[i]);
        int flags = jack_port_flags(port);
        if (flags & JackPortIsInput) continue;

        if ((connections = jack_port_get_all_connections (client, jack_port_by_name(client, ports[i]))) != 0) {
            for (int j = 0; connections[j]; j++) {
                print_connection(ports[i], connections[j]);
            }
            jack_free (connections);
        }
    }

    if (ports) {
        jack_free (ports);
    }
    jack_client_close (client);
    exit (0);
}
