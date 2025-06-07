#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stdbool.h>

#include "defs.h"
#include "vset.h"

/**
 * Funções para processar/manipular o arquivo de dados,
 * bem como lidar com os registros do arquivo.
 */

/**
 * Inicializa o registro de cabeçalho `header` com os
 * valores padrão para um arquivo vazio. Os valores
 * iniciais para os campos de descrição não são
 * necessariamente válidos e devem ser sobrescritos.
 */
void file_init_header(f_header_t *header);

/**
 * Lê o registro de cabeçalho a partir da posição atual no
 * arquivo `f` e armazena-o em `header`.
 */
bool file_read_header(FILE *f, f_header_t *header);

/**
 * Escreve o registro de cabeçalho apontado por `header` no
 * arquivo `f`, na posição atual.
 */
bool file_write_header(FILE *f, const f_header_t *header);

/**
 * Lê um registro de dados a partir da posição atual no arquivo
 * `f` e armazena-o em `rec`, usando os códigos para os campos de
 * tamanho variável definidos no registro de cabeçalho `header`.
 *
 * Antes de usar os demais campos do registro, deve-se verificar o valor
 * do campo `removed`: se for `REC_REMOVED`, os outros campos não foram
 * lidos (não devem ser acessados).
 */
bool file_read_data_rec(FILE *f, const f_header_t *header, f_data_rec_t *rec);

/**
 * Escreve o registro de dados apontado por `rec` no arquivo
 * `f`, na posição atual, usando os códigos para os campos de
 * tamanho variável definidos no registro de cabeçalho `header`.
 */
bool file_write_data_rec(FILE *f, const f_header_t *header, const f_data_rec_t *rec);

/**
 * Realiza uma busca sequencial por um registro cujos valores correspondam ao
 * de `vset` no arquivo `f`, a partir da posição atual. Retorna `-1` se nenhum
 * registro for encontrado. Caso contrário, retorna o offset do registro no
 * arquivo e o armazena em `*rec`.
 *
 * `*rec` deve ser um registro devidamente inicializado: em particular,
 * os campos de tamanho variável devem admitir valores `NULL` ou valores que já
 * foram liberados usando `free`, para evitar vazamento de memória.
 *
 * Se um dos campos envolvidos na busca (cujo valor foi retornado no registro)
 * não permitir repetições, `*unique` será `true`.
 */
int64_t file_search_seq_next(FILE *f, const f_header_t *header, vset_t *vset, f_data_rec_t *rec, bool *unique);

/**
 * Imprime os campos de dados do registro `rec`, usando as descrições
 * contidas no registro de cabeçalho `header`.
 */
void file_print_data_rec(const f_header_t *header, const f_data_rec_t *rec);

/**
 * Apaga os campos de tamanho variável do registro `rec`.
 */
void rec_free_var_data_fields(f_data_rec_t *rec);

#endif /* FILE_H */
