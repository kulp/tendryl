#include "tendryl.h"
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#define REALLOC(X,N)    (ops->realloc((X), (N)))
#define ALLOC(N)        REALLOC(NULL, N)
#define FREE(X)         REALLOC(X, 0)
#define CALLOC(X, N, M) ALLOC((N)*(M))

typedef struct cp_info cp_info;
typedef struct field_info field_info;
typedef struct method_info method_info;
typedef struct attribute_info attribute_info;

struct ClassFile {
    u4 magic;
    u2 minor_version;
    u2 major_version;
    u2 constant_pool_count;
    cp_info **constant_pool;
    u2 access_flags;
    u2 this_class;
    u2 super_class;
    u2 interfaces_count;
    u2 *interfaces;
    u2 fields_count;
    field_info **fields;
    u2 methods_count;
    method_info **methods;
    u2 attributes_count;
    attribute_info **attributes;
};

struct cp_info {
    int tag;
    union {
        struct {
            u2 name_index;
        } C;
    } info;
};

static inline u4 GET4(FILE *f)
{
    u1 t[4];
    fread(t, 1, 4, f);
    return t[0] << 24 | t[1] << 16 | t[2] << 8 | t[3];
}

static inline u2 GET2(FILE *f)
{
    u1 t[2];
    fread(t, 1, 2, f);
    return t[0] << 8 | t[1];
}

static inline u1 GET1(FILE *f)
{
    u1 t[1];
    fread(t, 1, 1, f);
    return t[0];
}

int tendryl_parse_classfile(FILE *f, tendryl_ops *ops, tendryl_class **_c)
{
    int rc = 0;

    tendryl_class *c = *_c = ALLOC(sizeof *c);

    // TODO make parse_magic
    {
        c->magic = GET4(f);
        if (c->magic != 0xCAFEBABE)
            return ops->error(EINVAL, "bad magic %#x", c->magic);
    }

    // TODO make a parse_version
    {
        c->minor_version = GET2(f);
        c->major_version = GET2(f);
        int rc = ops->version(c->major_version, c->minor_version);
        if (rc)
            return ops->error(rc, "rejected version %d.%d", c->major_version, c->minor_version);
    }

    {
        c->constant_pool_count = GET2(f);
        c->constant_pool = ALLOC(c->constant_pool_count * sizeof *c->constant_pool);
        for (unsigned i = 1; i < c->constant_pool_count; i++) // notice 1-indexing
            ops->parse.cp_info(f, ops, &c->constant_pool[i]);
    }

    return rc;
}

static int check_version(u2 major, u2 minor)
{
    return 0; // return errno if rejected
}

static int parse_cp_info(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC(sizeof *cp);
    cp->tag = GET1(f);
    if (cp->tag < CONSTANT_min || cp->tag >= CONSTANT_max)
        return ops->error(EINVAL, "invalid constant pool tag %d", cp->tag);

    if (ops->parse.dispatch[cp->tag])
        return ops->parse.dispatch[cp->tag](f, ops, &cp->info);
    else
        return ops->error(EFAULT, "missing handler for constant pool tag %d", cp->tag);

    return -1;
}

static int got_error(int code, const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    vfprintf(stderr, fmt, vl);
    fputs("\n", stderr);
    va_end(vl);
    errno = code;
    return -1;
}

int tendryl_init_ops(tendryl_ops *ops)
{
    ops->realloc = realloc;
    ops->error = got_error;
    ops->verbose = printf;

    ops->version = check_version;
    ops->parse = (struct tendryl_parsers){
        .cp_info = parse_cp_info,
    };

    return -1;
}

/* vi: set et ts=4 sw=4: */
