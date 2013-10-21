#include "tendryl.h"
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#define REALLOC(X,N)    (ops->realloc((X), (N)))
#define ALLOC(N)        REALLOC(NULL, N)
#define FREE(X)         REALLOC(X, 0)
#define CALLOC(X, N, M) ALLOC((N)*(M))

#define ALLOC_UPTO_LEN(C,F,N)       ALLOC(offsetof(C##_info,info.F) + (N))

#define ALLOC_CP_UPTO_LEN(F,N)      ALLOC_UPTO_LEN(cp,F,N)
#define ALLOC_CP_UPTO(F)            ALLOC_CP_UPTO_LEN(F,sizeof (cp_info){ .tag = 0 }.info.F)

#define ALLOC_AT_UPTO_LEN(F,N)      ALLOC_UPTO_LEN(attribute,F,N)
#define ALLOC_AT_UPTO(F)            ALLOC_CP_UPTO_LEN(F,sizeof (attribute_info){ .attribute_name_index = 0 }.info.F)

typedef struct cp_info cp_info;
typedef struct field_info field_info;
typedef struct method_info method_info;
typedef struct attribute_info attribute_info;

int TENDRYL_DEBUG_LEVEL = 0;

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
        struct cp_Class {
            u2 name_index;
        } C;
        struct cp_Fieldref /* or Methodref, or InterfaceMethodref */ {
            u2 class_index;
            u2 name_and_type_index;
        } FMI;
        struct cp_Utf8 {
            u2 length;
            u1 bytes[];
        } U;
        struct cp_NameAndType {
            u2 name_index;
            u2 descriptor_index;
        } NAT;
        struct cp_String {
            u2 string_index;
        } S;
        struct cp_Integer /* or Float */ {
            u4 bytes;
        } I;
        struct cp_Long /* or Double */ {
            u4 high_bytes;
            u4 low_bytes;
        } L;
    } info;
};

struct attribute_info {
    u2 attribute_name_index;
    u4 attribute_length;
    union {
        struct attr_ConstantValue {
            u2 constantvalue_index;
        } CV;
        struct attr_Code {
            u2 max_stack;
            u2 max_locals;

            u4 code_length;
            u1 *code;

            u2 exception_table_length;
            struct exception_entry {
                u2 start_pc;
                u2 end_pc;
                u2 handler_pc;
                u2 catch_type;
            } *exception_table;

            u2 attributes_count;
            attribute_info *attributes[];
        } C;
        struct attr_Exceptions {
            u2 number_of_exceptions;
            u2 exception_index_table[];
        } E;
    } info;
};

struct field_info {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 attributes_count;
    attribute_info *attributes[];
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

    {
        c->fields_count = GET2(f);
        c->fields = ALLOC(c->fields_count * sizeof *c->fields);
        for (unsigned i = 0; i < c->fields_count; i++)
            ops->parse.field_info(f, ops, &c->fields[i]);
    }

    {
        c->methods_count = GET2(f);
        c->methods = ALLOC(c->methods_count * sizeof *c->methods);
        for (unsigned i = 0; i < c->methods_count; i++)
            ops->parse.method_info(f, ops, &c->methods[i]);
    }

    {
        c->attributes_count = GET2(f);
        c->attributes = ALLOC(c->attributes_count * sizeof *c->attributes);
        for (unsigned i = 0; i < c->attributes_count; i++)
            ops->parse.attribute_info(f, ops, &c->attributes[i]);
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

    if (ops->parse.pool[type]) {
        // TODO we would like the dispatch to know what tag it has, but
        // there's no space allocated yet. This would make parse_Methodref's
        // overloading more useful in .verbose(), but for now it is
        // non-critical. We could do something as naïve as realloc() if
        // it becomes necessary.
        int rc = ops->parse.pool[type](f, ops, _cp);
        (*(cp_info**)_cp)->tag = type;
        if (rc < 0)
            return rc;
         else
            return (type == CONSTANT_Long || type == CONSTANT_Double) ? 2 : 1;
    } else {
        return ops->error(EFAULT, "missing handler for constant pool tag %d", type);
    }

    return -1;
}

// parse_Methodref also handles Fieldref and InterfaceMethodref
static int parse_Methodref(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO(FMI.name_and_type_index);
    u2 ci = cp->info.FMI.class_index = GET2(f);
    u2 ni = cp->info.FMI.name_and_type_index = GET2(f);
    // TODO checking index validities
    return ops->verbose("Field/Method/InterfaceMethod ref with class index %d, name.type index %d", ci, ni);
}

static int parse_Class(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO(C.name_index);
    u2 ni = cp->info.C.name_index = GET2(f);
    return ops->verbose("Class with name index %d", ni);
}

static int parse_Utf8(FILE *f, tendryl_ops *ops, void *_cp)
{
    u2 len = GET2(f);
    // allocate one extra byte for a NUL char, for our own debugging use only
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO_LEN(U.bytes, len + 1);
    cp->info.U.length = len;
    fread(&cp->info.U.bytes, 1, len, f);
    cp->info.U.bytes[len] = '\0';
    return ops->verbose("Utf8 with length %d = `%s`", len, cp->info.U.bytes);
}

static int parse_NameAndType(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO(NAT.descriptor_index);
    u2 ni = cp->info.NAT.name_index = GET2(f);
    u2 di = cp->info.NAT.descriptor_index = GET2(f);
    return ops->verbose("NameAndType with name index %d, descriptor index %d", ni, di);
}

static int parse_String(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO(S.string_index);
    u2 si = cp->info.S.string_index = GET2(f);
    return ops->verbose("String with string index %d", si);
}

// parse_Integer handles Float as well
static int parse_Integer(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO(I.bytes);
    u4 iv = cp->info.I.bytes = GET4(f);
    return ops->verbose("Integer/Float with bytes %#x", iv);
}

// parse_Long handles Double as well
static int parse_Long(FILE *f, tendryl_ops *ops, void *_cp)
{
    cp_info *cp = *(cp_info **)_cp = ALLOC_CP_UPTO(L.low_bytes);
    u4 hv = cp->info.L.high_bytes = GET4(f);
    u4 lv = cp->info.L.low_bytes = GET4(f);
    return ops->verbose("Long/Double with bytes %#llx", ((long long)hv) << 32 | lv);
}

// parse_field_info also handles method_info
static int parse_field_info(FILE *f, tendryl_ops *ops, void *_fi)
{
    // allocate a default number of attributes, and realloc after
    field_info *fi = *(field_info **)_fi = ALLOC(16 * sizeof *fi->attributes + sizeof *fi);
    u2 af = fi->access_flags = GET2(f);
    u2 ni = fi->name_index = GET2(f);
    u2 di = fi->descriptor_index = GET2(f);
    u2 ac = fi->attributes_count = GET2(f);
    fi = REALLOC(fi, ac * sizeof *fi->attributes + sizeof *fi);
    for (unsigned i = 0; i < fi->attributes_count; i++)
        ops->parse.attribute_info(f, ops, &fi->attributes[i]);

    return ops->verbose("field_info with access %#x, name index %d, descriptor index %d, and %d attributes", af, ni, di, ac);
}

static unsigned int attr_hash(const char *str, unsigned int len)
{
    static unsigned char asso_values[] = {
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 30, 41, 41, 41, 15, 41, 41, 41, 41,
         0,  0, 41, 41, 41, 41, 10,  0, 41, 41,
        25,  0, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41
    };

    return len + asso_values[(unsigned char)str[1]];
}

static const char *attr_wordlist[] = {
    "", "", "", "",
    "Code",
    "", "", "", "",
    "Synthetic",
    "SourceFile",
    "",
    "InnerClasses",
    "ConstantValue",
    "",
    "EnclosingMethod",
    "BootstrapMethods",
    "AnnotationDefault",
    "LocalVariableTable",
    "",
    "SourceDebugExtension",
    "",
    "LocalVariableTypeTable",
    "StackMapTable",
    "Signature",
    "RuntimeVisibleAnnotations",
    "",
    "RuntimeInvisibleAnnotations",
    "", "",
    "LineNumberTable",
    "", "", "",
    "RuntimeVisibleParameterAnnotations",
    "Exceptions",
    "RuntimeInvisibleParameterAnnotations",
    "", "", "",
    "Deprecated"
};

static enum attribute_type attr_lookup(const char *str, unsigned int len)
{
    // enums for scoped symbolic use
    enum { MIN_WORD_LENGTH=4, MAX_WORD_LENGTH=36 };
    enum { MIN_HASH_VALUE=4, MAX_HASH_VALUE=40 };

    if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
        enum attribute_type key = attr_hash(str, len);

        if (key <= MAX_HASH_VALUE) {
            const char *s = attr_wordlist[key];

            if (*str == *s && !strcmp(str + 1, s + 1))
                return key;
        }
    }

    return 0;
}

static int parse_attribute_info(FILE *f, tendryl_ops *ops, void *_ai)
{
    u2 ni = GET2(f);
    u4 al = GET4(f);
    attribute_info *ai = *(attribute_info **)_ai = ALLOC(al + sizeof *ai);
    ai->attribute_name_index = ni;
    ai->attribute_length = al;
    // TODO check for valid index
    struct cp_Utf8 *u = &ops->clazz->constant_pool[ni]->info.U;
    enum attribute_type at = attr_lookup((const char *)u->bytes, u->length);
    if (ops->parse.attr[at]) {
        ops->parse.attr[at](f, ops, ai);
    } else {
        ops->parse.attr[ATTRIBUTE_invalid](f, ops, ai);
    }

    return ops->verbose("attribute_info with name index %d:%s=%d:%s and length %d"
                        , ni, u->bytes, at, attr_wordlist[at], al);
}

static int parse_attribute_invalid(FILE *f, tendryl_ops *ops, void *_at)
{
    attribute_info *ai = _at;
    fread(&ai->info, 1, ai->attribute_length, f);
    int ni = ai->attribute_name_index;
    struct cp_Utf8 *u = &ops->clazz->constant_pool[ni]->info.U;
    enum attribute_type at = attr_lookup((const char *)u->bytes, u->length);
    // "A Java Virtual Machine implementation is required to silently ignore
    // any or all attributes in the attributes table of a ClassFile structure
    // that it does not recognize." JVM Spec section 4.1
    // Thus we emit this as debug information, not as normal verbose output
    return ops->debug(1, "unhandled attribute %d:%s", at, attr_wordlist[at]);
}

static int parse_attribute_Code(FILE *f, tendryl_ops *ops, void *_at)
{
    attribute_info *ai = _at;
    struct attr_Code *ac = &ai->info.C;
    ac->max_stack = GET2(f);
    ac->max_locals = GET2(f);
    ac->code_length = GET4(f);
    ac->code = ALLOC(ac->code_length * sizeof *ac->code);
    fread(&ac->code, 1, ac->code_length, f);
    ac->exception_table_length = GET2(f);
    ac->exception_table = ALLOC(ac->exception_table_length * sizeof *ac->exception_table);
    for (unsigned i = 0; i < ac->exception_table_length; i++) {
        struct exception_entry *e = &ac->exception_table[i];
        e->start_pc = GET2(f);
        e->end_pc = GET2(f);
        e->handler_pc = GET2(f);
        e->catch_type = GET2(f);
    }
    ac->attributes_count = GET2(f);
    // ac->attributes is allocated by the original attribute_length alloc
    for (unsigned i = 0; i < ac->attributes_count; i++)
        ops->parse.attribute_info(f, ops, &ac->attributes[i]);

    return ops->verbose("Code with overall length %d, bytecode length %d"
                        , ai->attribute_length, ac->code_length);
}

static int parse_attribute_ConstantValue(FILE *f, tendryl_ops *ops, void *_at)
{
    attribute_info *ai = _at;
    struct attr_ConstantValue *ac = &ai->info.CV;
    u2 ci = ac->constantvalue_index = GET2(f);
    return ops->verbose("ConstantValue with index %d", ci);
}

static int parse_attribute_Exceptions(FILE *f, tendryl_ops *ops, void *_at)
{
    attribute_info *ai = _at;
    struct attr_Exceptions *e = &ai->info.E;
    u2 ne = e->number_of_exceptions = GET2(f);
    // e->exceptions is allocated by the original attribute_length alloc
    for (unsigned i = 0; i < ne; i++)
        e->exception_index_table[i] = GET2(f);

    return ops->verbose("Exceptions with count %d", ne);
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

static int got_debug(int level, const char *fmt, ...)
{
    if (level <= TENDRYL_DEBUG_LEVEL) {
        va_list vl;
        va_start(vl, fmt);
        vprintf(fmt, vl);
        fputs("\n", stdout);
        va_end(vl);
    }
    return 0;
}

int tendryl_init_ops(tendryl_ops *ops)
{
    ops->realloc = realloc;
    ops->error = got_error;
    ops->verbose = got_verbose;
    ops->debug = got_debug;

    ops->version = check_version;
    ops->parse = (struct tendryl_parsers){
        .classfile = parse_classfile,
        .cp_info = parse_cp_info,
        .pool = {
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
        .field_info = parse_field_info,
        .method_info = parse_field_info,
        .attribute_info = parse_attribute_info,
        .attr = {
            [ATTRIBUTE_invalid]       = parse_attribute_invalid,
            [ATTRIBUTE_Code]          = parse_attribute_Code,
            [ATTRIBUTE_ConstantValue] = parse_attribute_ConstantValue,
            [ATTRIBUTE_Exceptions]    = parse_attribute_Exceptions,
        },
    };

    return -1;
}

/* vi: set et ts=4 sw=4: */
