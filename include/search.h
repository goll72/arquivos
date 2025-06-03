#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>
#include <stdint.h>

#include "defs.h"
#include "query.h"

/**
 * Realiza uma busca sequencial por um registro que atenda à query `query`
 * no arquivo `f`, a partir da posição atual. Retorna `-1` se nenhum registro
 * for encontrado. Caso contrário, retorna o offset do registro no arquivo
 * e o armazena em `*rec`.
 *
 * `*rec` deve ser um registro devidamente inicializado: em particular,
 * os campos de tamanho variável devem admitir valores `NULL` para evitar
 * vazamento de memória.
 *
 * Se um dos campos envolvidos na busca (cujo valor foi retornado no registro)
 * não permitir repetições, `*unique` será `true`.
 */
int64_t file_search_seq_next(FILE *f, const f_header_t *header, query_t *query, f_data_rec_t *rec, bool *unique);

#endif /* SEARCH_H */
