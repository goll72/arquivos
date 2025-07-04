#ifndef CRUD_H
#define CRUD_H

#include <stdio.h>
#include <stdbool.h>

#include "defs.h"
#include "vset.h"

/**
 * Funcionalidades comumente usadas em bancos de dados,
 * como inserção, remoção e atualização de registros.
 */

/**
 * Insere o registro `rec` no arquivo `f`, usando o algoritmo
 * de reaproveitamento de espaço para determinar onde será
 * feita a inserção e fazendo os devidos ajustes no registro
 * de cabeçalho `header`. Retorna `true` se a inserção ocorreu
 * com sucesso, escrevendo em `*offset` o offset onde a inserção
 * foi realizada.
 */
bool crud_insert(FILE *f, f_header_t *header, f_data_rec_t *rec, uint64_t *offset);

/**
 * Remove o registro que se encontra na posição atual do arquivo `f`,
 * adicionando-o à lista de registros logicamente removidos e fazendo
 * os devidos ajustes no registro de cabeçalho `header`.
 */
bool crud_delete(FILE *f, f_header_t *header, f_data_rec_t *rec);

/**
 * Atualiza o registro na posição atual do arquivo `f`, cujo conteúdo
 * deve já estar armazenado em `*rec`, de acordo com os valores armazenados
 * no vset `patch` e fazendo os devidos ajustes no registro de cabeçalho `header`.
 *
 * Tenta realizar um update in-place (na mesma posição), se isso não for possível,
 * realiza uma remoção seguida por uma inserção.
 *
 * Retorna `true` se a atualização ocorreu com sucesso, escrevendo em `*offset` o
 * offset onde a atualização foi realizada.
 */
bool crud_update(FILE *f, f_header_t *header, f_data_rec_t *rec, vset_t *patch, uint64_t *offset);

#endif /* CRUD_H */
