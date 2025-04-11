#ifndef TYPEINFO_H
#define TYPEINFO_H

#include <stdint.h>

/**
 * Define os tipos suportados por esse sistema rudimentar
 * de reflexão de tipos.
 */

enum typeinfo {
    T_U32,
    T_FLT,
    T_STR
};

#define GET_TYPEINFO(T)  \
    _Generic(T,          \
        uint32_t: T_U32, \
        float:    T_FLT, \
        char *:   T_STR)

#endif /* TYPEINFO_H */
