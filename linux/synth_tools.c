#include "m_pd.h"

/* SHARED */
#define DEF_TILDE_CLASS(cname, ...) do {        \
        cname##_class = class_new(              \
            gensym(#cname "~"),                 \
            (t_newmethod)cname##_new,           \
            (t_method)cname##_free,             \
            sizeof(struct cname),               \
            0,                                                          \
            __VA_ARGS__,0);                                             \
        CLASS_MAINSIGNALIN(cname##_class, struct cname, x_f);           \
        class_addmethod(cname##_class, (t_method)cname##_dsp, gensym("dsp"), 0); \
    } while(0)

#define DEF_METHOD(cname,mname,...) \
    class_addmethod(cname##_class, (t_method)cname##_##mname, gensym(#mname), __VA_ARGS__, 0)


/* CLASSES */
t_class *square_grain_class;
struct square_grain {
    t_object x_obj;
    t_float x_f;
    t_float brightness;
};
static void *square_grain_new(t_floatarg brightness) {
    /* create instance */
    struct square_grain *x = (void *)pd_new(square_grain_class);
    x->brightness = brightness;
    /* Create inlets. */
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("brightness"));
    /* create a dsp outlet */
    outlet_new(&x->x_obj, gensym("signal"));
    return x;
}
static void square_grain_free(void) {
}
static t_int *square_grain_perform(t_int *w) {
    /* interpret DSP program */
    struct square_grain *s= (struct square_grain *)(w[1]);
    t_int    n   = (t_int)(w[2]);
    t_float *in  = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    for (t_int i=0; i<n; i++) {
        out[i] = in[i] * s->brightness;
    }
    /* pointer to next DSP program */
    return (w+5);
}
static void square_grain_brightness(struct square_grain *s, t_floatarg val) {
    post("brightness %f", val);
    s->brightness = val;
}
static void square_grain_dsp(struct square_grain *x, t_signal **sp) {
    post("square_grain_dsp");
    int n = sp[0]->s_n;
    t_float *in = sp[0]->s_vec;
    t_float *out = sp[1]->s_vec;
    dsp_add(square_grain_perform, 4, x, n, in, out);
}

void square_grain_setup(void) {
    DEF_TILDE_CLASS(square_grain, A_DEFFLOAT);
    DEF_METHOD(square_grain, brightness, A_FLOAT);
}

/* LIB SETUP */
#define SYNTH_TOOLS_VERSION "git"
#define CALL_SETUP(name) name##_setup();
#define DECL_SETUP(name) void name##_setup(void);
#define FOR_CLASS_TILDE(m)                      \
    m(square_grain)
FOR_CLASS_TILDE(DECL_SETUP)
void synth_tools_setup(void) {
    post("synth_tools: version " SYNTH_TOOLS_VERSION);
    post("t_int %d, t_float %d", sizeof(t_int), sizeof(t_float));
    FOR_CLASS_TILDE(CALL_SETUP)
}
