#ifndef TENDRYL_H_
#define TENDRYL_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct tendryl_ops tendryl_ops;
typedef struct ClassFile tendryl_class;

typedef uint32_t u4;
typedef uint16_t u2;
typedef uint8_t  u1;

// a tendryl_parser takes as its third argument a pointer to pointer (cast to
// void*), which is filled in with an allocation done by the tendryl_parser
// using the realloc() style allocator specified in tendryl_ops
typedef int tendryl_parser(FILE *f, tendryl_ops *ops, void *build);

enum constant_tag {
    CONSTANT_invalid             = 0,

#define CONSTANT_min CONSTANT_Utf8
    CONSTANT_Utf8                = 1,
    // no 2
    CONSTANT_Integer             = 3,
    CONSTANT_Float               = 4,
    CONSTANT_Long                = 5,
    CONSTANT_Double              = 6,
    CONSTANT_Class               = 7,
    CONSTANT_String              = 8,
    CONSTANT_Fieldref            = 9,
    CONSTANT_Methodref           = 10,
    CONSTANT_InterfaceMethodref  = 11,
    CONSTANT_NameAndType         = 12,
    // no 13
    // no 14
    CONSTANT_MethodHandle        = 15,
    CONSTANT_MethodType          = 16,
    // no 17
    CONSTANT_InvokeDynamic       = 18,

    CONSTANT_max
};

struct tendryl_ops {
    void *(*realloc)(void *orig, size_t size);
    int (*error)(int err, const char *fmt, ...);
    int (*verbose)(const char *fmt, ...);
    int (*version)(u2 major, u2 minor);
    struct tendryl_parsers {
        tendryl_parser *classfile, *cp_info, *dispatch[CONSTANT_max];
        tendryl_parser *field_info, *method_info, *attribute_info;
    } parse;
};

int tendryl_init_ops(tendryl_ops *ops);

#endif

/* vi: set et ts=4 sw=4: */
