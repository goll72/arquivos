#include "typeflags.h"

/**
 * Define os campos do registro de dados, de forma que a macro
 * `METADATA_FIELD` possa ser usada para manipular os campos de
 * metadados (todos de tamanho fixo), a macro `FIXED_FIELD` para
 * manipular os campos de dados de tamanho fixo e a macro
 * `VAR_FIELD` para os campos de tamanho variável.
 *
 * As macros passadas como argumento recebem os seguintes parâmetros:
 *
 *   - T: tipo do campo
 *   - name: nome do campo
 *   - repr: string usada para representar o campo
 *   - _: ignorado
 */

#ifndef METADATA_FIELD
#define METADATA_FIELD(T, name, _)
#endif

#ifndef FIXED_FIELD
#define FIXED_FIELD(T, name, repr, flags)
#endif

#ifndef VAR_FIELD
#define VAR_FIELD(T, name, repr)
#endif

/** Assume o valor `REC_REMOVED` se o registro foi removido. */
METADATA_FIELD(uint8_t,  removed,           _)

/**
 * Indica o tamanho do registro em disco (ignorando os campos
 * que antecedem o campo `size`, bem como o campo `size` em si)
 */
METADATA_FIELD(uint32_t, size,              _)

/**
 * Próximo registro logicamente removido na pilha de registros
 * logicamente removidos. Assume valor -1 se não houver um próximo.
 */
METADATA_FIELD(int64_t,  next_removed_rec,  _)

/** Identificador do ataque. */
FIXED_FIELD(uint32_t,    attack_id,         "idAttack",      F_UNIQUE)

/** Ano em que o ataque ocorreu. */
FIXED_FIELD(uint32_t,    year,              "year",          0)

/** Prejuízo causado pelo ataque. */
FIXED_FIELD(float,       financial_loss,    "financialLoss", 0)

/** País em que o ataque ocorreu. */
VAR_FIELD(char *,        country,           "country")

/** Tipo de ataque cibernético. */
VAR_FIELD(char *,        attack_type,       "attackType")

/** Setor da indústria afetado. */
VAR_FIELD(char *,        target_industry,   "targetIndustry")

/** Mecanismo de defesa empregado para resolver o problema. */
VAR_FIELD(char *,        defense_mechanism, "defenseMechanism")

#undef METADATA_FIELD
#undef FIXED_FIELD
#undef VAR_FIELD
