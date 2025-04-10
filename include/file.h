#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stdbool.h>

#include "defs.h"

/**
 * Inicializa o arquivo `f`, escrevendo o registro de cabeçalho
 * com os valores padrão na posição atual.
 */
bool file_init(FILE *f);

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
 * `f` e armazena-o em `reg`, usando os códigos para os campos de
 * tamanho variável definidos no registro de cabeçalho `header`.
 */
bool file_read_data_reg(FILE *f, const f_header_t *header, f_data_reg_t *reg);

/**
 * Escreve o registro de dados apontado por `reg` no arquivo
 * `f`, na posição atual, usando os códigos para os campos de
 * tamanho variável definidos no registro de cabeçalho `header`.
 */
bool file_write_data_reg(FILE *f, const f_header_t *header, const f_data_reg_t *reg);

/**
 * Imprime os campos de dados do registro `reg`, usando as descrições
 * contidas no registro de cabeçalho `header`.
 */
void file_print_data_reg(const f_header_t *header, const f_data_reg_t *reg);

#endif /* FILE_H */
