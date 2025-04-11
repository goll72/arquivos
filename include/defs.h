#ifndef DEFS_H
#define DEFS_H

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "typeinfo.h"

/**
 * Define os campos do registro de cabeçalho, de forma
 * que uma X macro pode ser passada como parâmetro para
 * manipular os campos de forma arbitrária (escrever ou
 * ler cada campo em um arquivo, por exemplo)
 *
 * A X macro passada como argumento tem os seguintes
 * parâmetros:
 *
 * X(T, name, default)
 *     T: tipo do campo
 *     name: nome do campo
 *     default: valor padrão do campo
 */
#define HEADER_REG_FIELDS(X)                                                      \
    X(uint8_t,  status,                 '0')                                      \
    X(int64_t,  top,                     -1)                                      \
    X(uint64_t, next_byte_offset,         0)                                      \
    X(uint32_t, n_valid_regs,             0)                                      \
    X(uint32_t, n_removed_regs,           0)                                      \
    X(char[23], attack_id_desc,         "IDENTIFICADOR DO ATAQUE")                \
    X(char[27], year_desc,              "ANO EM QUE O ATAQUE OCORREU")            \
    X(char[28], financial_loss_desc,    "PREJUIZO CAUSADO PELO ATAQUE")           \
    X(uint8_t,  country_code,           '1')                                      \
    X(char[26], country_desc,           "PAIS ONDE OCORREU O ATAQUE")             \
    X(uint8_t,  attack_type_code,       '2')                                      \
    X(char[38], attack_type_desc,       "TIPO DE AMEACA A SEGURANCA CIBERNETICA") \
    X(uint8_t,  target_industry_code,   '3')                                      \
    X(char[38], target_industry_desc,   "SETOR DA INDUSTRIA QUE SOFREU O ATAQUE") \
    X(uint8_t,  defense_mechanism_code, '4')                                      \
    X(char[67], defense_mechanism_desc, "ESTRATEGIA DE DEFESA CIBERNETICA EMPREGADA PARA RESOLVER O PROBLEMA")

/**
 * Define os campos do registro de dados, de forma
 * que a macro `X` pode ser usada para manipular os
 * campos de metadados de tamanho fixo, a macro `Y`
 * para manipular os campos de dados de tamanho fixo
 * e a macro `Z` para os campos de tamanho variável.
 * As macros passadas como argumento recebem os
 * seguintes parâmetros:
 *
 * X(T, name)
 *     T: tipo do campo
 *     name: nome do campo
 *
 * Y/Z(T, name, repr)
 *     T: tipo do campo
 *     name: nome do campo
 *     repr: string usada para representar o campo
 */
#define DATA_REG_FIELDS(X, Y, Z)                     \
    X(uint8_t,  removed)                             \
    X(uint32_t, size)                                \
    X(int64_t,  next_removed_reg)                    \
    Y(uint32_t, attack_id,         "idAttack")       \
    Y(uint32_t, year,              "year")           \
    Y(float,    financial_loss,    "financialLoss")  \
    Z(char *,   country,           "country")        \
    Z(char *,   attack_type,       "attackType")     \
    Z(char *,   target_industry,   "targetIndustry") \
    Z(char *,   defense_mechanism, "defenseMechanism")

/* Define a X macro que define os campos da struct */
#define X(T, name, ...) typeof(T) name;
#define Y(...)

typedef struct {
    HEADER_REG_FIELDS(X)
} f_header_t;

typedef struct {
    DATA_REG_FIELDS(X, X, X)
} f_data_reg_t;

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

typedef struct {
    HEADER_REG_FIELDS(X)
} __attribute__((packed)) PACKED(f_header_t);

/* Apenas os campos de tamanho fixo */
typedef struct {
    DATA_REG_FIELDS(X, X, Y)
} __attribute__((packed)) PACKED(f_data_reg_t);

#undef X
#undef Y

/**
 * Escreve em `*offset` e `*info`, respectivamente, o offset
 * e o "tipo" do campo de `f_data_reg` cuja representação
 * na forma de string é `field_repr`.
 *
 * Retorna `false` se `field_repr` for inválido.
 */
static inline bool data_reg_typeinfo(const char *field_repr, size_t *offset, enum typeinfo *info)
{
    if (!field_repr)
        return false;
    
#define X(T, name, repr)                        \
    if (strcmp(field_repr, repr) == 0) {        \
        *offset = offsetof(f_data_reg_t, name); \
        *info = GET_TYPEINFO(T);                \
                                                \
        return true;                            \
    }

#define Y(...)

    DATA_REG_FIELDS(Y, X, X)
    
#undef X
#undef Y

    return false;
}

#endif /* DEFS_H */
