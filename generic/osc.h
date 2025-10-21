#ifndef  OSC_H
#define  OSC_H

#include "macros.h"
#include "scan.h"
#include <stdint.h>

#if 0
#undef LOG
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#endif

/* Minimal OSC implementation. */

/* Basic osc structure:
   - str: hierarchical address
   - str: payload type tags
   - binary payload

   All strings are 0-padded to 4-byte boundary.
 */

// FIXME: Use safe string methods.

union osc {
    char    s[4];
    float   f;
    int32_t i;
};
CT_ASSERT(osc_size, sizeof(union osc) == sizeof(uint32_t));

struct param;

struct param_context {
    const struct param * const* root;
};

/* Parameters are in a tree structure. */
typedef void (*osc_set_float)(struct param_context *, float);
typedef void (*osc_set_int)  (struct param_context *, int32_t);

typedef const struct param param_t;
typedef const struct param *const param_list_t;

/* All pointers in this struct point to const data since the struct is
   intended to go into Flash memory. */
struct param;
struct param {
    const char *name;
    uintptr_t type;
    union {
        // leaf value setters
        const osc_set_float set_f;
        const osc_set_int   set_i;
        // leaf value raw pointers
        float   *ptr_f;
        int32_t *ptr_i;
        // null-terminated array of param pointers
        // const struct param *const *list;
        param_list_t *list;
        // FIXME: Add explicit FLOAT_SETTER, FLOAT_POINTER variants
    } cont;
};
#define OSC_TYPE_LIST      0
#define OSC_TYPE_SET_FLOAT 1
#define OSC_TYPE_SET_INT   2
#define OSC_TYPE_PTR_FLOAT 3
#define OSC_TYPE_PTR_INT   4



static inline int osc_word(int n) {
    return ((n-1)/4)+1;
}

// Length and setters are separate so caller can alloc.
static inline uintptr_t osc_len_scalar(uintptr_t addr_len) {
    uintptr_t n_slots = osc_word(addr_len+1);
    return n_slots + 2;
}


static inline uintptr_t osc_make_scalar(union osc *msg,   uintptr_t msg_len,
                                        const char *addr, uintptr_t addr_len,
                                        char type) {
    // msg_len as allocated by osc_len_scalar
    uintptr_t i_type    = msg_len - 2;
    uintptr_t i_payload = msg_len - 1;
    memset((void*)msg, 0, msg_len * sizeof(msg[0]));
    memcpy((void*)msg, addr, addr_len);
    msg[i_type].s[0] = ',';
    msg[i_type].s[1] = type;
    return i_payload;
}
static inline void osc_make_float(union osc *msg,   uintptr_t msg_len,
                                  const char *addr, uintptr_t addr_len,
                                  float val) {
    uintptr_t i = osc_make_scalar(msg, msg_len, addr, addr_len, 'f');
    msg[i].f = val;
}
static inline void osc_make_int(union osc *msg,   uintptr_t msg_len,
                                const char *addr, uintptr_t addr_len,
                                int32_t val) {
    uintptr_t i = osc_make_scalar(msg, msg_len, addr, addr_len, 'i');
    msg[i].i = val;
}

static inline const struct param *osc_find(const struct param * const* pl, const char *name) {
    // LOG("find: %s\n", name);
    for(;;){
        const struct param *p = pl[0]; // There is always a NULL terminator.
        if (!p) return NULL;
        // LOG("- check %s\n", p->name);
        if (!strcmp(name, p->name)) return p;
        pl++;
    }
}

#define OSC_PARSE_OK           0
#define OSC_PARSE_MISSING      1
#define OSC_PARSE_WILDCARD     2
#define OSC_PARSE_NOT_FOUND    3
#define OSC_PARSE_EXTRA_ADDR   4
#define OSC_PARSE_BAD_TYPE     5
#define OSC_PARSE_NOT_SCALAR   6
#define OSC_PARSE_EXPECT_INT   7
#define OSC_PARSE_EXPECT_FLOAT 8
#define OSC_PARSE_UNKNOWN_TYPE 9
#define OSC_PARSE_EMPTY        10
#define OSC_PARSE_TEXT_MISSING 11
#define OSC_PARSE_EXTRA_TEXT   12


static inline const char *osc_error(int e) {
    static const char *errors[] = {
        [ OSC_PARSE_OK ]           = "success",
        [ OSC_PARSE_MISSING ]      = "missing address component",
        [ OSC_PARSE_WILDCARD ]     = "wildcard not supported",
        [ OSC_PARSE_NOT_FOUND ]    = "parameter not found",
        [ OSC_PARSE_EXTRA_ADDR ]   = "extra address component",
        [ OSC_PARSE_BAD_TYPE ]     = "bad type syntax",
        [ OSC_PARSE_NOT_SCALAR ]   = "only supporting scalars",
        [ OSC_PARSE_EXPECT_INT ]   = "expected integer",
        [ OSC_PARSE_EXPECT_FLOAT ] = "expected float",
        [ OSC_PARSE_UNKNOWN_TYPE ] = "unknown type tag",
        [ OSC_PARSE_EMPTY ]        = "empty address",
        [ OSC_PARSE_TEXT_MISSING ] = "missing number in text string",
        [ OSC_PARSE_EXTRA_TEXT ]   = "extra data in text string",
    };
    if (e < 0) return "unknown";
    if (e > ARRAY_SIZE(errors)) return "unknown";
    return errors[e];
}

static inline int osc_parse_addr(struct param_context *x, const char *addr, const struct param **pp, int *pi_type) {
    const struct param * const* pl = x->root;
    // LOG("root0: %s\n", pl[0]->name);

    int n = strlen(addr)+1;
    char buf[n];
    strcpy(buf, addr);

    struct scan scan;
    scan_init(&scan, buf, '/');

    char *tok;

  next:
    if (!(tok = scan_next(&scan))) {
        return OSC_PARSE_MISSING;
    }
    // LOG("tok: %s\n", tok);
    if (tok[0] == '*') {
        return OSC_PARSE_WILDCARD;
    }
    const struct param *p = osc_find(pl, tok);
    if (!p) {
        return OSC_PARSE_NOT_FOUND;
    }
    // LOG("found %s type=%d\n", tok, p->type);
    if (p->type == OSC_TYPE_LIST) {
        /* Recurse tree */
        pl = p->cont.list;
        goto next;
    }
    else {
        /* Handle leaf.  Input address needs to be at the end */
        if ((tok = scan_next(&scan))) {
            // LOG("extra tok %s\n", tok);
            return OSC_PARSE_EXTRA_ADDR;
        }
        /* Input addr is complete and the param tree it at a leaf
           node. */
        *pp = p;
        *pi_type = osc_word(n);
        return 0;
    }
    // Not reached.
}






static inline int osc_parse(struct param_context *x, const union osc *cmd, uintptr_t nb_cmd ) {
    const struct param *p;
    int i_type;
    int e = osc_parse_addr(x, cmd->s, &p, &i_type);
    if (e) return e;
    const char *t = cmd[i_type].s;
    // LOG("type: %s\n", t);
    if (t[0] != ',') {
        return OSC_PARSE_BAD_TYPE;
    }
    if (t[2] != 0) {
        LOG("only supporting scalars\n");
        return OSC_PARSE_NOT_SCALAR;
    }
    const union osc *w = &cmd[i_type+1];
    switch(p->type) {
    case OSC_TYPE_SET_FLOAT:
        /* Expecting float */
        if (t[1] != 'f') {
            return OSC_PARSE_EXPECT_FLOAT;
        }
        // LOG("float: %f\n", w->f);
        p->cont.set_f(x, w->f);
        return OSC_PARSE_OK;
    case OSC_TYPE_SET_INT:
        /* Expecting int */
        if (t[1] != 'i') {
            return OSC_PARSE_EXPECT_INT;
        }
        // LOG("int: %d\n", w->i);
        p->cont.set_i(x, w->i);
        return OSC_PARSE_OK;
    case OSC_TYPE_PTR_FLOAT:
        /* Expecting float */
        if (t[1] != 'f') {
            return OSC_PARSE_EXPECT_FLOAT;
        }
        // LOG("float: %f\n", w->f);
        *p->cont.ptr_f = w->f;
        return OSC_PARSE_OK;
    case OSC_TYPE_PTR_INT:
        /* Expecting int */
        if (t[1] != 'i') {
            return OSC_PARSE_EXPECT_INT;
        }
        // LOG("int: %d\n", w->i);
        *p->cont.ptr_i = w->i;
        return OSC_PARSE_OK;
    default:
        LOG("bad type %d\n", p->type);
        return OSC_PARSE_UNKNOWN_TYPE;
    }
    /* Not reached */
}

/* This is a text representation of the protocol for use cases where
   binary is just too cumbersome to use.  Stay as close as possible to
   the original. */

static inline int osc_parse_set_number(struct param_context *x,
                                       const struct param *p, const char *number) {
    switch(p->type) {
    case OSC_TYPE_SET_FLOAT: {
        float f = atof(number);
        // LOG("float: %f\n", f);
        p->cont.set_f(x, f);
        return OSC_PARSE_OK;
    }
    case OSC_TYPE_SET_INT: {
        uint32_t i = atoi(number);
        // LOG("int: %d\n", i);
        p->cont.set_i(x, i);
        return OSC_PARSE_OK;
    }
    case OSC_TYPE_PTR_FLOAT: {
        float f = atof(number);
        // LOG("float: %f\n", f);
        *p->cont.ptr_f = f;
        return OSC_PARSE_OK;
    }
    case OSC_TYPE_PTR_INT: {
        uint32_t i = atoi(number);
        // LOG("int: %d\n", i);
        *p->cont.ptr_i = i;
        return OSC_PARSE_OK;
    }
    default:
        LOG("bad type %d\n", p->type);
        return OSC_PARSE_UNKNOWN_TYPE;
    }
}




// Note that this does not buffer partial lines.
#ifndef OSC_LINE_MAX
#define OSC_LINE_MAX 255
#endif




/* Traverse the tree to visit all atom nodes + pass in a (revrsed) path. */
struct osc_rev_path;
struct osc_rev_path {
    struct osc_rev_path *parent;
    const char *name;
};
typedef void (*osc_visit_fn)(struct param_context *,
                             struct osc_rev_path *, const struct param *);

static inline void osc_traverse_pl(
    struct param_context *x,
    osc_visit_fn visit,
    struct osc_rev_path *path,
    const struct param * const* pl)
{
    for (; *pl; pl++) {
        const struct param *p = *pl;
        if (p->type != OSC_TYPE_LIST) {
            visit(x, path, p);
        }
        else {
            struct osc_rev_path path1 = {
                .parent = path,
                .name = p->name,
            };
            osc_traverse_pl(x, visit, &path1, p->cont.list);
        }
    }
}

static inline void osc_traverse(struct param_context *x,
                                osc_visit_fn visit) {
    osc_traverse_pl(x, visit, NULL, x->root);
}


struct osc_fwd_path;
struct osc_fwd_path {
    struct osc_fwd_path *child;
    const char *name;
};
static inline void osc_log_fwd_path(const struct osc_fwd_path *path) {
    for(const struct osc_fwd_path *p = path; p; p = p->child) {
        LOG("/%s", p->name);
    }
}
static inline void osc_log_rev_path(const struct osc_rev_path *path) {
    if (path) {
        osc_log_rev_path(path->parent);
        LOG("/%s", path->name);
    }
}


/* For wildcard access. */

static inline int osc_parse_path_set_number(
    struct param_context *x,
    const struct osc_fwd_path *w_path, /* with wildcards */
    const struct param* const* pl,
    struct osc_rev_path *a_path, /* actual path, reversed */
    const char *number,
    int *count)
{
    if (!w_path) return OSC_PARSE_MISSING;

    int wildcard = w_path->name[0] == '*';  // FIXME: generalize matching
    for(;;){
        const struct param *p = pl[0]; // There is always a NULL terminator.
        if (!p) return 0;
        // LOG("- check %s\n", p->name);
        if (wildcard || (!strcmp(w_path->name, p->name))) {
            // LOG("match: %s %s\n", w_path->name, p->name);
            struct osc_rev_path a_path1 = {
                .name = p->name,
                .parent = a_path,
            };
            if (p->type == OSC_TYPE_LIST) {
                osc_parse_path_set_number(
                    x, w_path->child, p->cont.list, &a_path1, number, count);
            }
            else {
                /* It's a leaf node.  Require that path is complete. */
                if (w_path->child != NULL) return OSC_PARSE_EXTRA_ADDR;

                /* Set the data.  Thos will succeed because we know
                   it's not OSC_TYPE_LIST. */
                // LOG("leaf: "); osc_log_rev_path(&a_path1); LOG("\n");
                int rv = osc_parse_set_number(x, p, number);
                (void)rv;
                if (count) (*count)++;
            }
        }
        pl++;
    }
    return OSC_PARSE_OK;
}

static inline int osc_parse_text_set_number(struct param_context *x,
                                            const char *wc_addr,
                                            const char *number) {
    /* The wildcard tree traversal needs backtracking on the path
       which is easier to represent using pointers into an immutable
       data structure, so first scan the path into a linked list. */

    char buf[strlen(wc_addr)+1];
    strcpy(buf, wc_addr);

    struct scan scan;
    scan_init(&scan, buf, '/');

    struct osc_fwd_path *path = NULL;
    struct osc_fwd_path **ppath = &path;

    char *tok;
    while ((tok = scan_next(&scan))) {
        // LOG("tok %s\n", tok);
        struct osc_fwd_path *path1 = alloca(sizeof(*path1));
        path1->name = tok;
        path1->child = NULL;
        *ppath = path1;
        ppath = &path1->child;
    }

    if (0) {
        osc_log_fwd_path(path);
    }

    const struct param * const* pl = x->root;

    int count = 0;
    int rv = osc_parse_path_set_number(x, path, pl, NULL, number, &count);
    if (rv) return rv;
    if (count == 0) {
        // Before wildcards, no match would be an error.  Keep that
        // behavior?
        return OSC_PARSE_NOT_FOUND;
    }
    else {
        return OSC_PARSE_OK;
    }
}

static inline int osc_parse_text(struct param_context *x, const char *line) {

    /* String is processed in-place, so make a copy. */
    int n = strlen(line)+1;
    char buf[n];
    memcpy(buf, line, n);

    struct scan scan;
    scan_init(&scan, buf, ' ');

    char *wc_addr;
    if (!(wc_addr = scan_next(&scan))) {
        return OSC_PARSE_EMPTY;
    }
    // LOG("wc_addr = %s\n", wc_addr);

    char *number;
    if (!(number = scan_next(&scan))) {
        return OSC_PARSE_TEXT_MISSING;
    }

    char *extra;
    if ((extra = scan_next(&scan))) {
        return OSC_PARSE_EXTRA_TEXT;
    }

    /* Set each node that matches the wildcard expression in wc_addr
       string to value specified in number string. */
    return osc_parse_text_set_number(x, wc_addr, number);

}

static inline void osc_parse_text_lines(
    struct param_context *x, uint8_t *buf, uintptr_t len)
{
    text_for_lines((text_for_lines_fn)osc_parse_text,
                   x, buf, len, OSC_LINE_MAX);
}




#ifndef OSC_STATIC
#define OSC_STATIC static
#endif

/* Param setters */
#define DEF_OSC_SET_FLOAT(_cname, _name, _fun)                              \
    OSC_STATIC const struct param _cname = {.name = _name, .type = OSC_TYPE_SET_FLOAT, .cont = { .set_f = _fun }}
#define DEF_OSC_SET_INT(_cname, _name, _fun)                                \
    OSC_STATIC const struct param _cname = {.name = _name, .type = OSC_TYPE_SET_INT,   .cont = { .set_i = _fun }}

/* Raw pointers */
#define DEF_OSC_PTR_FLOAT(_cname, _name, _ptr)                              \
    OSC_STATIC const struct param _cname = {.name = _name, .type = OSC_TYPE_PTR_FLOAT, .cont = { .ptr_f = _fun }}
#define DEF_OSC_PTR_INT(_cname, _name, _ptr)                                \
    OSC_STATIC const struct param _cname = {.name = _name, .type = OSC_TYPE_PTR_INT,   .cont = { .ptr_i = _fun }}


#define DEF_OSC_LIST(_cname, _name, ...)                                \
    OSC_STATIC const struct param *const _cname##_list[] = {__VA_ARGS__ , NULL};               \
    OSC_STATIC const struct param _cname = {.name = _name, .type = OSC_TYPE_LIST, .cont = { .list = _cname##_list }};


#endif
