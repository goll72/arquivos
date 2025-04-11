#ifndef QUERY_H
#define QUERY_H

#include <stdlib.h>
#include <stdbool.h>

/**
 * Queries com um sistema de tipos extremamente rudimentar.
 * Há apenas dois tipos: string ou não-string.
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
 * Adiciona uma condição de igualdade à query. `str` deve ser `true`
 * se o campo relevante for uma string (`char *`). Nesse caso, a
 * comparação será feita dereferenciando o conteúdo da posição
 * `offset` como um `char *` e levando em consideração o delimitador '\0'.
 *
 * `offset` corresponde ao offset, em bytes, da região de memória
 * relevante a ser comparada.
 *
 * `buf` é a região de memória referência para a comparação. Deve
 * ser alocado dinamicamente. Passará a pertencer à query e será
 * desalocado usando `free` ao usar `query_free`.
 *
 * `len` indica quantos bytes serão comparados se `str` for `false`
 * e quantos bytes, no máximo, serão comparados se `str` for `true`.
 */
void query_add_cond_equals(query_t *query, bool str, size_t offset, void *buf, size_t len);

/**
 * Retorna `true` se todas as condições adicionadas
 * à query `query` são verdadeiras para `obj`.
 */
bool query_matches(query_t *query, void *obj);

#endif
