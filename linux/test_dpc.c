#include "mod_hub_devices.c"
void test_dpc_to_sel(int d, int p, int c) {
    int sel = dpc_to_sel(d, p, c);
    LOG("(%d,%d,%d)->%d\n", d, p, c, sel);
}
int main(int argc, char **argv) {
    test_dpc_to_sel(0,0,0);
    test_dpc_to_sel(1,0,0);
    test_dpc_to_sel(0,2,0);
    return 0;
}
