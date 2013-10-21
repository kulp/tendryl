#include "tendryl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    tendryl_ops _ops, *ops = &_ops;
    tendryl_init_ops(ops);
    TENDRYL_DEBUG_LEVEL = (unsigned short)-1;
    ops->parse.classfile(f, ops, &ops->clazz);
    fclose(f);

    return EXIT_SUCCESS;
}

/* vi: set et ts=4 sw=4: */
