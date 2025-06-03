#ifndef TYPEINFO_H
#define TYPEINFO_H

#include <stdint.h>

/**
 * Define os tipos suportados por esse sistema rudimentar
 * de reflexão de tipos.
 *
 * Usado para permitir que o código de query e parsing de
 * strings seja mais genérico.
 */

enum typeinfo {
    T_U32,
    T_FLT,
    T_STR
};

#define GET_TYPEINFO(T)  \
    _Generic((T){0},     \
        uint32_t: T_U32, \
        float:    T_FLT, \
        char *:   T_STR)

#include "typeflags.h"

#endif /* TYPEINFO_H */
