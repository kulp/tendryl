#ifndef TENDRYL_H_
#define TENDRYL_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

extern int TENDRYL_DEBUG_LEVEL;

typedef struct tendryl_ops tendryl_ops;
typedef struct ClassFile tendryl_class;

typedef uint32_t u4;
typedef uint16_t u2;
typedef uint8_t  u1;

// a tendryl_parser takes as its third argument a pointer to pointer (cast to
// void*), which is filled in with an allocation done by the tendryl_parser
// using the realloc() style allocator specified in tendryl_ops
typedef int tendryl_parser(FILE *f, tendryl_ops *ops, void *build);
typedef struct attribute_info attribute_info;
typedef int tendryl_attr_parser(FILE *f, tendryl_ops *ops, attribute_info *attr);

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

enum attribute_type {
    ATTRIBUTE_invalid = 0,

#define ATTRIBUTE_min ATTRIBUTE_Code
    // enum values come from gperf hash ; ideally they would not be exposed
    // TODO consider hiding ATTRIBUTE_* values and exposing only ATTRIBUTE_max
    ATTRIBUTE_Code = 4,
    ATTRIBUTE_Synthetic = 9,
    ATTRIBUTE_SourceFile = 10,
    ATTRIBUTE_InnerClasses = 12,
    ATTRIBUTE_ConstantValue = 13,
    ATTRIBUTE_EnclosingMethod = 15,
    ATTRIBUTE_BootstrapMethods = 16,
    ATTRIBUTE_AnnotationDefault = 17,
    ATTRIBUTE_LocalVariableTable = 18,
    ATTRIBUTE_SourceDebugExtension = 20,
    ATTRIBUTE_LocalVariableTypeTable = 22,
    ATTRIBUTE_StackMapTable = 23,
    ATTRIBUTE_Signature = 24,
    ATTRIBUTE_RuntimeVisibleAnnotations = 25,
    ATTRIBUTE_RuntimeInvisibleAnnotations = 27,
    ATTRIBUTE_LineNumberTable = 30,
    ATTRIBUTE_RuntimeVisibleParameterAnnotations = 34,
    ATTRIBUTE_Exceptions = 35,
    ATTRIBUTE_RuntimeInvisibleParameterAnnotations = 36,
    ATTRIBUTE_Deprecated = 40,

    ATTRIBUTE_max
};

struct tendryl_ops {
    tendryl_class *clazz;

    void *(*realloc)(void *orig, size_t size);
    int (*error)(int err, const char *fmt, ...);
    int (*debug)(int level, const char *fmt, ...);
    int (*version)(u2 major, u2 minor);
    struct tendryl_parsers {
        tendryl_parser *classfile, *cp_info, *pool[CONSTANT_max];
        tendryl_parser *field_info, *method_info, *attribute_info;
        tendryl_attr_parser *attr[ATTRIBUTE_max];
    } parse;
};

int tendryl_init_ops(tendryl_ops *ops);

#endif

/* vi: set et ts=4 sw=4: */
