#include "macros.h"
int main (int argc, char **argv) {
    float arr[3][4];
    LOG("%p\n", &arr[0]);
    LOG("%p\n", &arr[1]);
}
