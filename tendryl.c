#include "tendryl.h"
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stddef.h>

#define REALLOC(X,N)    (ops->realloc((X), (N)))
#define ALLOC(N)        REALLOC(NULL, N)
#define FREE(X)         REALLOC(X, 0)
#define CALLOC(X, N, M) ALLOC((N)*(M))

#define ALLOC_UPTO_PLUS(F,N)    ALLOC(offsetof(cp_info,info.F) + sizeof (cp_info){ .tag = 0 }.info.F)
#define ALLOC_UPTO(F)           ALLOC_UPTO_PLUS(F,0)

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
        struct {
            u2 class_index;
            u2 name_and_type_index;
        } FMI;
        struct {
            u2 length;
            u1 bytes[0]; // needs to permit sizeof, so not a flexible array
        } U;
        struct {
            u2 name_index;
            u2 descriptor_index;
        } NAT;
        struct {
            u2 string_index;
        } S;
        struct {
            u4 bytes;
        } I;
        struct {
            u4 high_bytes;
            u4 low_bytes;
        } L;
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

static int parse_classfile(FILE *f, tendryl_ops *ops, void *_c)
{
    int rc = 0;

    tendryl_class *c = *(tendryl_class **)_c = ALLOC(sizeof *c);

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
        int inc = -1;
        for (unsigned i = 1; i < c->constant_pool_count; i += inc) { // notice 1-indexing
            inc = ops->parse.cp_info(f, ops, &c->constant_pool[i]);
            if (inc < 0) // if inc < 0, it's an error ; otherwise, it's the increment
                return inc;
        }
    }

    {
        c->access_flags = GET2(f);
        c->this_class = GET2(f);
        c->super_class = GET2(f);
    }

    {
        c->interfaces_count = GET2(f);
        c->interfaces = ALLOC(c->interfaces_count * sizeof *c->interfaces);
        for (unsigned i = 0; i < c->interfaces_count; i++)
            c->interfaces[i] = GET2(f);
    }

    return rc;
}

static int check_version(u2 major, u2 minor)
{
    return 0; // return errno if rejected
}

static int parse_cp_info(FILE *f, tendryl_ops *ops, void *_cp)
{
    u1 type = GET1(f);
    if (type < CONSTANT_min || type >= CONSTANT_max)
        return ops->error(EINVAL, "invalid constant pool tag %d", type);

    if (ops->parse.dispatch[type]) {
        // TODO we would like the dispatch to know what tag it has, but
        // there's no space allocated yet. This would make parse_Methodref's
        // overloading more useful in .verbose(), but for now it is
        // non-critical. We could do something as naïve as realloc() if
        // it becomes necessary.
        int rc = ops->parse.dispatch[type](f, ops, _cp);
        (*(cp_info**)_cp)->tag = type;
        if (rc < 0)
            return rc;
         else
             switch (type) {
                case CONSTANT_Long:
                case CONSTANT_Double:
                    return 2;
                default:
                    return 1;
             }
    } else {
        return ops->error(EFAULT, "missing handler for constant pool tag %d", type);
    }

    return -1;
}

// parse_Methodref also handles Fieldref and InterfaceMethodref
static int parse_Methodref(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO(FMI.name_and_type_index);
    u2 ci = cp->info.FMI.class_index = GET2(f);
    u2 ni = cp->info.FMI.name_and_type_index = GET2(f);
    // TODO checking index validities
    return ops->verbose("Field/Method/InterfaceMethod ref with class index %d, name.type index %d", ci, ni);
}

static int parse_Class(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO(C.name_index);
    u2 ni = cp->info.C.name_index = GET2(f);
    return ops->verbose("Class with name index %d", ni);
}

static int parse_Utf8(FILE *f, tendryl_ops *ops, void *_cp)
{
    u2 len = GET2(f);
    // allocate one extra byte for a NUL char, for our own debugging use only
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO_PLUS(U.bytes, len + 1);
    cp->info.U.length = len;
    fread(&cp->info.U.bytes, 1, len, f);
    cp->info.U.bytes[len] = '\0';
    return ops->verbose("Utf8 with length %d = `%s`", len, cp->info.U.bytes);
}

static int parse_NameAndType(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO(NAT.descriptor_index);
    u2 ni = cp->info.NAT.name_index = GET2(f);
    u2 di = cp->info.NAT.descriptor_index = GET2(f);
    return ops->verbose("NameAndType with name index %d, descriptor index %d", ni, di);
}

static int parse_String(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO(S.string_index);
    u2 si = cp->info.S.string_index = GET2(f);
    return ops->verbose("String with string index %d", si);
}

// parse_Integer handles Float as well
static int parse_Integer(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO(I.bytes);
    u4 iv = cp->info.I.bytes = GET4(f);
    return ops->verbose("Integer/Float with bytes %#x", iv);
}

// parse_Long handles Double as well
static int parse_Long(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_UPTO(L.low_bytes);
    u4 hv = cp->info.L.high_bytes = GET4(f);
    u4 lv = cp->info.L.low_bytes = GET4(f);
    return ops->verbose("Long/Double with bytes %#llx", ((long long)hv) << 32 | lv);
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

static int got_verbose(const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    vprintf(fmt, vl);
    fputs("\n", stdout);
    va_end(vl);
    return 0;
}

int tendryl_init_ops(tendryl_ops *ops)
{
    ops->realloc = realloc;
    ops->error = got_error;
    ops->verbose = got_verbose;

    ops->version = check_version;
    ops->parse = (struct tendryl_parsers){
        .classfile = parse_classfile,
        .cp_info = parse_cp_info,
        .dispatch = {
            [CONSTANT_Utf8]               = parse_Utf8,
            [CONSTANT_Integer]            = parse_Integer,
            [CONSTANT_Float]              = parse_Integer,
            [CONSTANT_Long]               = parse_Long,
            [CONSTANT_Double]             = parse_Long,
            [CONSTANT_Class]              = parse_Class,
            [CONSTANT_String]             = parse_String,
            [CONSTANT_Fieldref]           = parse_Methodref,
            [CONSTANT_Methodref]          = parse_Methodref,
            [CONSTANT_InterfaceMethodref] = parse_Methodref,
            [CONSTANT_NameAndType]        = parse_NameAndType,
            // TODO MethodHandle
            // TODO MethodType
            // TODO InvokeDynamic
        },
    };

    return -1;
}

/* vi: set et ts=4 sw=4: */
