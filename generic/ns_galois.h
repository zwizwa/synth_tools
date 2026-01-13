// See test_fft.
typedef uint32_t NS(_data_t);
typedef uint32_t NS(_real_t);
static inline void NS(_data_mul3)(NS(_data_t) *o,
                                  const NS(_data_t) *a,
                                  const NS(_data_t) *b) {
    *o = ((*a) * (*b)) % NS(_field_mod);
}
static inline void NS(_data_mac3)(NS(_data_t) *acc,
                                  const NS(_data_t) *a,
                                  const NS(_data_t) *b) {
    *acc = ((*acc) + (*a) * (*b)) % NS(_field_mod);
}
static inline void NS(_data_acc2)(NS(_data_t) *o,
                                  const NS(_data_t) *a) {
    *o = ((*o) + (*a)) % NS(_field_mod);
}

static inline void NS(_init_coefs)(NS(_data_t) *c, int logn) {
    ASSERT(c);
    int n = 1<<logn;
    c[0] = 1;
    NS(_data_t) generator = 3;
#if 1
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

static inline void NS(_from_real)(NS(_data_t) *o, const NS(_real_t) *a) {
    *o = *a;
}
static inline void NS(_to_real)(NS(_real_t) *o, const NS(_data_t) *a) {
    *o = *a;
}

static inline void NS(_log_data_vec)(NS(_data_t) *v, int size) {
    for (int k=0; k<size; k++) {
        LOG(" %d", v[k]);
    }
    LOG("\n");
}
static inline void NS(_log_real_vec)(NS(_real_t) *v, int size) {
    NS(_log_data_vec)((void*)v, size); // same
}
