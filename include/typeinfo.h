#ifndef TYPEINFO_H
#define TYPEINFO_H

#include <stdint.h>

/**
 * Define os tipos suportados por esse sistema rudimentar
 * de reflexão de tipos.
 *
 * Usado para definir uma abstração de "vset" --- um conjunto
 * de valores, onde cada valor tem um determinado tipo
 * e um offset associado com relação a um objeto base (nesse
 * caso, a struct `f_data_rec_t` do registro de dados). Por sua
 * vez, essa abstração é usada para realizar comparações na busca.
 *
 * Além disso, esses tipos também são usados para unificar o
 * código de parsing de strings.
 */

enum typeinfo {
    T_U32,
    T_FLT,
    T_STR
};

/**
 * Permite que o valor do `enum typeinfo` correspondente a um
 * determinado tipo em C seja obtido em tempo de compilação.
 */
#define GET_TYPEINFO(T)  \
    _Generic((T){0},     \
        uint32_t: T_U32, \
        float:    T_FLT, \
        char *:   T_STR)

#endif /* TYPEINFO_H */
