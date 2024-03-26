#include "m_pd.h"
#include <math.h>

/* Next:

   Add a loader for "double buffered" dynamic objects.

*/


/* SHARED */
#define DEF_TILDE_CLASS(cname, ...) do {                                \
        cname##_class = class_new(                                      \
            gensym(#cname "~"),                                         \
            (t_newmethod)cname##_new,                                   \
            (t_method)cname##_free,                                     \
            sizeof(struct cname), 0,                                    \
            __VA_ARGS__,0);                                             \
        CLASS_MAINSIGNALIN(cname##_class, struct cname, x_f);           \
        class_addmethod(cname##_class, (t_method)cname##_dsp, gensym("dsp"), 0); \
    } while(0)

#define DEF_CLASS(cname, ...) do {                                      \
        cname##_class = class_new(                                      \
            gensym(#cname),                                             \
            (t_newmethod)cname##_new,                                   \
            (t_method)cname##_free,                                     \
            sizeof(struct cname), 0,                                    \
            __VA_ARGS__,0);                                             \
    } while(0)

#define DEF_METHOD(cname,mname,...) \
    class_addmethod(cname##_class, (t_method)cname##_##mname, gensym(#mname), __VA_ARGS__, 0)


/* CLASSES */

/*  Combine putch detection and granual synthesis / convolution.

    Idea is to take the input signal and distort it to square/pulse
    wave, then isolate the dicontinuities and map them to
    grain-triggering or a proper convolution with the differential
    (positive and negative diract pulses).

    Then in a second pass, use the average (pulse density) to modify
    the timbre of the grain.  E.g. stretch it to match pitch.

    This should give a near perfect pitch mapping, and smooth out the
    timbre mapping over a longer period of time because it is assumed
    to be not so important.

    To make this work the time resolution should probably be very
    high, i.e. much more than 44.1kHz

    Convolution is likely overkill, but a properly sampled grain is
    probably a good idea.

    How to test?

    Start from a bass lick.

    Perform prefiltering and interpolated zero crossing to emulate
    high resolution analog preprocessing step.

    Then implement the grain playback at normal sample frequency with
    interpolation.

    To detect zero crossings, use a Schmitt Trigger.  That takes care
    of noise right away.

    Next: use the input energy/level to set the output level.  Add
    some prefiltering to filter out up to 400Hz or so.


*/

t_class *square_grain_class;
struct square_grain {
    t_object x_obj;
    t_float x_f;
    t_float brightness;
    t_float threshold;
    t_float state;
};
static inline void square_grain_proc(
    struct square_grain *s, t_int n, t_float *in, t_float *out) {
    t_float state = s->state;
    t_float thresh = s->threshold;
    for (t_int i=0; i<n; i++) {
        t_float val = in[i];
        out[i] = state;
        if ((state >= 0) && (val < -thresh)) {
            state = -0.5f;
        }
        else if ((state < 0) && (val > thresh)) {
            state = 0.5f;
        }
    }
    s->state = state;
}
static void square_grain_brightness(struct square_grain *s, t_floatarg val) {
    post("brightness %f", val);
    s->brightness = val;
}
static void square_grain_threshold(struct square_grain *s, t_floatarg val) {
    val = fabs(val);
    post("threshold %f", val);
    s->threshold = val;
}
static t_int *square_grain_perform(t_int *w) {
    /* interpret DSP program (w[0] points to _perform, reset is args */
    square_grain_proc(
        (struct square_grain *)(w[1]),
        (t_int)(w[2]),
        (t_float *)(w[3]),
        (t_float *)(w[4]));
    /* pointer to next DSP _perform */
    return (w+5);
}
static void square_grain_dsp(struct square_grain *x, t_signal **sp) {
    post("square_grain_dsp");
    int n = sp[0]->s_n;
    t_float *in = sp[0]->s_vec;
    t_float *out = sp[1]->s_vec;
    dsp_add(square_grain_perform, 4, x, n, in, out);
}
static void *square_grain_new(t_floatarg threshold) {
    /* create instance */
    struct square_grain *x = (void *)pd_new(square_grain_class);
    x->state = 0.0f;
    x->threshold = threshold;
    x->brightness = 1.0f;
    /* Create inlets. */
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("threshold"));
    /* create a dsp outlet */
    outlet_new(&x->x_obj, gensym("signal"));
    return x;
}
static void square_grain_free(void) {
}
void square_grain_setup(void) {
    DEF_TILDE_CLASS(square_grain, A_DEFFLOAT);
    DEF_METHOD(square_grain, brightness, A_FLOAT);
    DEF_METHOD(square_grain, threshold, A_FLOAT);
}

t_class *scale_class;
struct scale {
    t_object x_obj;
    t_outlet *out;
    t_float base, diff;
    void (*process)(struct scale *x, t_floatarg mfrac);
};
// FIXME: Everything is midi for now
static void scale_exp(struct scale *x, t_floatarg mfrac) {
    t_float frac = mfrac * (1.0f / 127.0f);
    t_float out = x->base * powf(x->diff, frac);
    outlet_float(x->out, out);
}
static void scale_lin(struct scale *x, t_floatarg mfrac) {
    t_float frac = mfrac * (1.0f / 127.0f);
    t_float out = x->base + x->diff * frac;
    outlet_float(x->out, out);
}
static void scale_float(struct scale *x, t_floatarg mfrac) {
    x->process(x, mfrac);
}
static void *scale_new(t_symbol *type, t_floatarg min, t_floatarg max) {
    /* create instance */
    struct scale *x = (void *)pd_new(scale_class);
    if (type == gensym("exp")) {
        x->base = min;
        x->diff = max / min;
        x->process = scale_exp;
    }
    else if (type == gensym("lin")) {
        x->base = min;
        x->diff = max - min;
        x->process = scale_lin;
    }
    else {
        return NULL;
    }
    /* main inlet handlers */
    /* create outlet */
    x->out = outlet_new(&x->x_obj, gensym("float"));
    return x;
}
static void scale_free(void) {
}
void scale_setup(void) {
    DEF_CLASS(scale, A_DEFSYMBOL, A_DEFFLOAT, A_DEFFLOAT);
    class_addfloat(scale_class, (t_method)scale_float);
}

/* exo interface helper object? */
t_class *exo_class;
struct exo {
    t_object x_obj;
    t_outlet *out;
};
static void *exo_new(t_symbol *type, t_floatarg min, t_floatarg max) {
    /* create instance */
    struct exo *x = (void *)pd_new(exo_class);
    /* main inlet handlers */
    /* create outlet */
    x->out = outlet_new(&x->x_obj, gensym("float"));
    return x;
}
static void exo_free(void) {
}
void exo_setup(void) {
    DEF_CLASS(exo, A_DEFSYMBOL, A_DEFFLOAT, A_DEFFLOAT);
}



/* LIB SETUP */
#define SYNTH_TOOLS_VERSION "git"
#define CALL_SETUP(name) name##_setup();
#define DECL_SETUP(name) void name##_setup(void);

#define FOR_CLASS_TILDE(m)                      \
    m(square_grain)                             \

#define FOR_CLASS(m)                            \
    m(scale)                                    \
    m(exo)                                      \

FOR_CLASS_TILDE(DECL_SETUP)
void synth_tools_setup(void) {
    post("synth_tools: version " SYNTH_TOOLS_VERSION);
    post("t_int %d, t_float %d, t_obj %d",
         sizeof(t_int), sizeof(t_float), sizeof(t_object));
    FOR_CLASS_TILDE(CALL_SETUP)
    FOR_CLASS(CALL_SETUP)
}
