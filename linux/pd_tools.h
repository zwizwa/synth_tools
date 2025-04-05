#ifndef PD_TOOLS
#define PD_TOOLS

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


#endif
