#include "m_pd.h"

/* SHARED */


/* CLASSES */
t_class *square_grain_class;
struct square_grain {
    t_object x_obj;
};
static void *square_grain_new(t_floatarg arg1, t_floatarg arg2) {
    /* create instance */
    struct square_grain *x = (void *)pd_new(square_grain_class);
    (void)x;
    /* create a dsp outlet */
    outlet_new(&x->x_obj, gensym("signal"));
    return x;
}
static void square_grain_free(void) {
}
static t_int *square_grain_perform(t_int *w) {
    /* interpret arguments */
    t_float *out  = (t_float *)(w[3]);
    struct square_grain *obj = (struct square_grain *)(w[1]);
    (void)out;
    (void)obj;
    /* return endx */
    return (w+4);
}
static void square_grain_dsp(struct square_grain *x, t_signal **sp) {
    post("square_grain_dsp\n");
    dsp_add(square_grain_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}
void square_grain_setup(void) {
    square_grain_class = class_new(
        gensym("square_grain~"),
        (t_newmethod)square_grain_new,
    	(t_method)square_grain_free,
        sizeof(struct square_grain),
        0,
        A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(square_grain_class, (t_method)square_grain_dsp, gensym("dsp"), 0);
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
    FOR_CLASS_TILDE(CALL_SETUP)
}
