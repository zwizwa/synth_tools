#include "macros.h"
#include "cgen_synth_out.h"
#include "cgen_test_out.h"


void test_integrator(void) {
    struct integrator_state s = {.s0 = 0};
    LOG("integrator:");
    for (int i=0; i<20; i++) {
        struct integrator_in in = { .i0 = i };
        struct integrator_out out;
        integrator_update(&s, &in, &out);
        LOG(" %d", (int)out.o0);
    }
    LOG("\n");
}
void test_procproc(void) {
    struct procproc_state s = {.s0 = 0, .s0 = 0};
    LOG("procproc:");
    for (int i=0; i<20; i++) {
        struct procproc_in in = { .i0 = i };
        struct procproc_out out;
        procproc_update(&s, &in, &out);
        LOG(" %d", (int)out.o0);
    }
    LOG("\n");
}
void test_sumramp(void) {
    struct sumramp_state s = {.s0 = 0, .s0 = 0};
    LOG("sumramp:");
    for (int i=0; i<20; i++) {
        struct sumramp_in in = { .i0 = i };
        struct sumramp_out out;
        sumramp_update(&s, &in, &out);
        LOG(" %d", (int)out.o0);
    }
    LOG("\n");
}
void test_matrix(void) {
    struct matrix_state s = {};
    LOG("matrix:");
    struct matrix_in in = {};
    struct matrix_out out;
    matrix_update(&s, &in, &out);
    for (int i=0; i<3; i++) {
        for (int j=0; j<4; j++) {
            LOG(" %d", (int)out.o0[i][j]);
        }
    }
    LOG("\n");
}

void test_timeloop(void) {
    struct timeloop_state s = {};
    LOG("timelloop");
    struct timeloop_in in = {};
    struct timeloop_out out;
    timeloop_update(&s, &in, &out);
    for (int t=0; t<20 /*ARRAY_SIZE(out.o0)*/; t++) {
        LOG(" %d", (int)out.o0[t]);
    }
    LOG("\n");
}

/* See rkt/test-ffi.rkt */
void test(void) {
    LOG("test_cgen.c\n");
    test_integrator();
    test_procproc();
    test_sumramp();
    test_matrix();
    test_timeloop();
}

#if 0
int main(int argc, char **argv) {
    // State is initialized to zero.
    struct synth_state state = {};
    for (int i=0; i<20; i++) {
        // Input is an array of phase increments, one for each osc.
        // Constant here, but this is a signal, it can very with each
        // update.
        struct synth_in in = { .i0 = {.3, .0001} };
        // Output is a single audio sample.
        struct synth_out out;
        synth_update(&state, &in, &out);
        printf("%f\n", out.o0);
    }
    return 0;
}
#endif
