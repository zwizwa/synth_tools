// See test_fft.
typedef uint32_t NS(_data_t);
static inline void NS(_data_mul3)(NS(_data_t) *o,
                                  const NS(_data_t) *a,
                                  const NS(_data_t) *b) {
    *o = ((*a) * (*b)) % NS(_field_mod);
}
static inline void NS(_data_add2)(NS(_data_t) *o,
                                  const NS(_data_t) *a) {
    *o = ((*o) + (*a)) % NS(_field_mod);
}

static inline void NS(_init_coefs)(NS(_data_t) *c, int logn) {
    ASSERT(c);
    int n = 1<<logn;
    c[0] = 1;
    NS(_data_t) generator = 3;
#if 0
    // Forward
    for (int i=1; i<n; i++) {
        NS(_data_mul3)(&c[i], &c[i-1], &generator);
        //LOG("%d %d\n", i, c[i]);
    }
#else
    // Reverse
    c[n-1] = generator;
    for (int i=n-2; i>0; i--) {
        NS(_data_mul3)(&c[i], &c[i+1], &generator);
        //LOG("%d %d\n", i, c[i]);
    }
#endif
}
