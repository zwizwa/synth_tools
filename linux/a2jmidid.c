/* This is a dedicated binary wrapper for a2jmidid, started by
   jack_client.erl

   We call into a script that is managed as part of /etc/net

   This .c file (and derived binary) replaces the old
   start_a2jmidid.sh script which used socat.
*/

#include "mod_exec.c"
int main(int argc, char **argv) {
    char *script = "a2jmidid.local";
    char *a2j_argv[] = { script, script };
    return exec_main(ARRAY_SIZE(a2j_argv), a2j_argv);
}
