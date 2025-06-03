#ifndef DEFS_H
#define DEFS_H

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "typeinfo.h"

/**
 * Definições dos registros usados no arquivo e outros metadados.
 */

#define STATUS_INCONSISTENT '0'
#define STATUS_CONSISTENT   '1'

#define REC_NOT_REMOVED '0'
#define REC_REMOVED     '1'

/* clang-format off */


/**
 * Definições de structs usadas.
 */

/* Define a X macro que define os campos da struct */
#define X(T, name, ...) typeof(T) name;

#define HEADER_FIELD   X

/**
 * Registro de cabeçalho (representação em memória primária)
 */
typedef struct {
    #include "x/header.h"
} f_header_t;

#define METADATA_FIELD X
#define FIXED_FIELD    X
#define VAR_FIELD      X

/**
 * Registro de dados (representação em memória primária)
 */
typedef struct {
    #include "x/data.h"
} f_data_rec_t;

/**
 * Tipos "packed"/"empacotados", sem espaçamento
 * entre os campos na sua representação. Não podem
 * ser usados de forma portátil para armazenar dados
 * na memória principal, mas permitem o cálculo de
 * offsets no arquivo em tempo de compilação, usando
 * a macro `offsetof`, uma vez que possuem a mesma
 * representação de um registro salvo no arquivo.
 *
 * (não é possível obter o offset de campos de tamanho variável)
 */
#define PACKED(T) packed_##T

#define HEADER_FIELD   X

typedef struct {
    #include "x/header.h"
} __attribute__((packed)) PACKED(f_header_t);

/* Apenas os campos de tamanho fixo */
#define METADATA_FIELD X
#define FIXED_FIELD    X

typedef struct {
    #include "x/data.h"
} __attribute__((packed)) PACKED(f_data_rec_t);

#undef X


/**
 * Tamanho da parte de tamanho fixa do registro de dados
 * que vem após o campo `size`. Usado para calcular o valor
 * desse campo ao criar um registro e para verificar se esse
 * valor é válido ao ler um registro.
 */
#define DATA_REC_SIZE_AFTER_SIZE_FIELD \
    (sizeof(PACKED(f_data_rec_t)) - offsetof(PACKED(f_data_rec_t), size) - sizeof(((PACKED(f_data_rec_t) *)0)->size))


/**
 * Escreve em `*offset` e `*info`, respectivamente, o offset
 * e o "tipo" do campo de `f_data_rec_t` cuja representação
 * na forma de string é `field_repr`.
 *
 * Retorna `false` se `field_repr` for inválido.
 */
static inline bool data_rec_typeinfo(const char *field_repr, size_t *offset, enum typeinfo *info, uint8_t *flags)
{
    if (!field_repr)
        return false;

    #define X(T, name, repr, flags) { repr, offsetof(f_data_rec_t, name), GET_TYPEINFO(T), flags },

    #define FIXED_FIELD(T, name, repr, flags) X(T, name, repr, flags)
    #define VAR_FIELD(T, name, repr)          X(T, name, repr, 0)

    static const struct {
        char *repr;
        size_t offset;
        enum typeinfo info;
        uint8_t flags;
    } info_arr[] = {
        #include "x/data.h"
    };

    #undef X

    // `field_repr` é comparado, um a um, com o valor de `repr` para cada um
    // dos campos até que seja encontrado um valor correspondente. Ou seja,
    // a busca realizada por essa função é da ordem de O(n), sendo n a
    // quantidade de strings (campos definidos no registro).
    //
    // É possível usar um hashmap para realizar a busca em O(1).
    for (int i = 0; i < sizeof info_arr / sizeof info_arr[0]; i++) {
        if (strcmp(field_repr, info_arr[i].repr) == 0) {
            *offset = info_arr[i].offset;
            *info = info_arr[i].info;
            *flags = info_arr[i].flags;

            return true;
        }
    }

    return false;
}

#endif /* DEFS_H */
