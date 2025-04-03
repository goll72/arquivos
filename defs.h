#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

/* Define os campos do registro de cabeçalho, de forma
 * que uma X macro pode ser passada como parâmetro para
 * manipular os campos de forma arbitrária (escrever ou
 * ler cada campo em um arquivo, por exemplo)
 */
#define HEADER_REG_FIELDS(X)               \
    X(uint8_t,  status)                    \
    X(int64_t,  top)                       \
    X(uint64_t, next_byte_offset)          \
    X(uint32_t, n_valid_regs)              \
    X(uint32_t, n_removed_regs)            \
    X(char[23], id_attack_desc)            \
    X(char[27], year_desc)                 \
    X(char[28], financial_loss_desc)       \
    X(uint8_t,  country_desc_code)         \
    X(char[26], country_desc)              \
    X(uint8_t,  attack_type_desc_code)     \
    X(char[38], attack_type_desc)          \
    X(uint8_t,  target_industry_desc_code) \
    X(char[38], target_industry_desc)      \
    X(uint8_t,  defense_desc_code)         \
    X(char[67], defense_desc)

/* Define os campos do registro de dados, de forma
 * que a macro `X` pode ser usada para manipular os
 * campos de tamanho fixo e a macro `Y` pode ser
 * usada para manipular os campos de tamanho variável
 */
#define DATA_REG_FIELDS(X, Y)      \
    X(uint8_t,  removed)           \
    X(uint32_t, size)              \
    X(int64_t,  next_removed_reg)  \
    X(uint32_t, attack_id)         \
    X(uint32_t, year)              \
    X(double,   financial_loss)    \
    Y(char *,   country)           \
    Y(char *,   attack_type)       \
    Y(char *,   target_industry)   \
    Y(char *,   defense_mechanism)

/* Define a X macro que define os campos da struct */
#define X(T, name) typeof(T) name;

typedef struct {
    HEADER_REG_FIELDS(X)
} f_header_t;

typedef struct {
    DATA_REG_FIELDS(X, X)
} f_data_reg_t;

#undef X

#endif /* DEFS_H */
