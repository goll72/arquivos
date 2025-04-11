#ifndef QUERY_H
#define QUERY_H

#include <stdlib.h>
#include <stdbool.h>

#include "typeinfo.h"

/**
 * Queries com um sistema de tipos rudimentar.
 *
 * Uma query é um conjunto de condições (de igualdade apenas),
 * que ao ser aplicado a um "objeto" (uma região de memória)
 * diz se as condições passam (realiza um E lógico).
 */

typedef struct query query_t;

/**
 * Cria uma query.
 */
query_t *query_new(void);

/**
 * Apaga uma query.
 */
void query_free(query_t *query);

/**
 * Adiciona uma condição de igualdade à query.
 *
 * Se o campo relevante for uma string (`T_STR`), a comparação
 * será feita dereferenciando o conteúdo do byte offset `offset`
 * da região de memória apontada por `obj`, tratando-o como um
 * `char *` e comparando o seu conteúdo byte-a-byte com a região
 * de memória apontada por `buf`, levando em consideração o
 * delimitador '\0'.
 *
 * `offset` corresponde ao offset, em bytes, da região de memória
 * relevante a ser comparada, em relação a `obj`.
 *
 * `buf` é a região de memória referência para a comparação. Deve
 * ser alocado dinamicamente. Passará a pertencer à query e será
 * desalocado ao usar `query_free`.
 */
void query_add_cond_equals(query_t *query, size_t offset, enum typeinfo info, void *buf);

/**
 * Retorna `true` se todas as condições adicionadas
 * à query `query` são verdadeiras para `obj`.
 */
bool query_matches(query_t *query, const void *obj);

#endif
