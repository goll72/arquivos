#ifndef VSET_H
#define VSET_H

#include <stdlib.h>
#include <stdbool.h>

#include "typeinfo.h"

/**
 * Um "vset" é um conjunto de valores de determinados campos
 * (arbitrários) de um objeto, representado como uma lista
 * contendo o valor em si, o tipo (um `enum typeinfo`) e uma
 * bitmask de flags de atributos.
 *
 * É usado para realizar comparação por igualdade na busca,
 * bem como para sobrescrita sistemática de campos na atualização.
 */

typedef struct vset vset_t;

/**
 * Cria um vset.
 */
vset_t *vset_new(void);

/**
 * Apaga um vset.
 */
void vset_free(vset_t *);

/**
 * Adiciona um valor ao vset.
 *
 * Se o campo relevante for uma string (`T_STR`), a comparação
 * será feita dereferenciando o conteúdo do byte offset `offset`
 * da região de memória apontada por `obj`, tratando-o como um
 * `char *` e comparando o seu conteúdo byte-a-byte com a região
 * de memória apontada por `buf`, levando em consideração o
 * delimitador '\0'.
 *
 * `offset` corresponde ao offset, em bytes, da região de memória
 * onde o valor deve ser armazenado, em relação a `obj`.
 *
 * `buf` é a região de memória que guarda o valor que será copiado
 * pelo vset. Deve ser alocado dinamicamente. Passará a pertencer
 * ao vset e será desalocado ao usar `vset_free`.
 */
void vset_add_value(vset_t *vset, size_t offset, enum typeinfo info, uint8_t typeflags, void *buf);

/**
 * Retorna `true` se todos os valores adicionados ao vset correspondem
 * aos valores de `obj`.
 */
bool vset_match_against(vset_t *vset, const void *obj, bool *unique);

/**
 * Modifica os valores dos campos de `obj`, alterando-os para os valores
 * especificados em `vset`. Para campos do tipo `T_STR`, aloca uma nova
 * string, se necessário, liberando a string armazenada anteriormente
 * naquela posição em `obj`, se houver.
 */
void vset_patch(vset_t *vset, void *obj);

#endif /* VSET_H */
