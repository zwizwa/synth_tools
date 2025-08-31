// Not clear about C semantcs.

#define N 2
#define M 3
void proc(const float in[N][M], float out[N][M]) {
    for (int n = 0; n < N; n++) {
        for (int m = 0; m < M; m++) {
            float r1 = in[n][m];
            float r2 = r1 * r1;
            out[n][m] = r2;
        }
    }
}

int main(int argc, char **argv) {
    const float in[N][M] = {};
    float out[N][M];
    proc(in, out);
}
