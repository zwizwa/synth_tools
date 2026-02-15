#include "macros.h"
#include "lace.h"
#include <unistd.h>

/* The point here is to execute a DSP flow graph.
   In the end I want to make
*/



TASK_1(int, fibonacci, int, n)  // macro to create Lace functions (can be in header file)

int fibonacci_CALL(lace_worker* lw, int n) {
    if(n < 2) return n;
    fibonacci_SPAWN(lw, n-1);         // SPAWN a task (fork)
    int a = fibonacci_CALL(lw, n-2);  // run another task in parallel
    int b = fibonacci_SYNC(lw);       // SYNC the spawned task (join)
    return a + b;
}

int main(int argc, char **argv) {
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    LOG("test_lace.c nproc=%d\n", (int)nprocs);
    int dqsize    = 0; // default task deque size
    int stacksize = 0; // default program stack size
    lace_start(nprocs, dqsize, stacksize);
    int result = fibonacci(42);
    printf("fibonacci(42) = %d\n", result);
    lace_stop();
    return 0;
}
