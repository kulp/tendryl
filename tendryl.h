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

// a tendryl_parser takes as its third argument a double pointer (cast to a
// void*), which is filled in with an allocation done by the tendryl_parser
// using the realloc() style allocator specified in tendryl_ops
typedef int tendryl_parser(FILE *f, tendryl_ops *ops, void *);

struct tendryl_ops {
    void *(*realloc)(void *orig, size_t size);
    int (*error)(int err, const char *fmt, ...);
    int (*verbose)(const char *fmt, ...);
    int (*version)(u2 major, u2 minor);
};

int tendryl_parse_classfile(FILE *f, tendryl_ops *ops, tendryl_class **c);
int tendryl_init_ops(tendryl_ops *ops);

#endif

/* vi: set et ts=4 sw=4: */
