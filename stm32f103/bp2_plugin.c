// generated by epid_cproc.erl
#define CPROC_NB_INPUTS 1
#include "mod_cproc_plugin.c"
void cproc_update(w *input) {
    LET(n1, proc_edge, .in = input[0]);
    LET(n2, proc_acc, .in = n1.out);
}
