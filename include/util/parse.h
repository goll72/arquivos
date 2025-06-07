#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdbool.h>

#include "typeinfo.h"

/** Funções para lidar com parsing de strings. */

enum f_type {
    F_TYPE_CSV,
    F_TYPE_UNDELIM,
};

/**
 * Lê um campo do arquivo de texto `f`, com tipo especificado por
 * `info`, armazenando o resultado na região de memória apontada
 * por `dest`. Se `dest` for `NULL`, a leitura é realizada, porém o
 * resultado não é armazenado.
 *
 * Caso `info` seja `T_STR`, dereferencia a região de memória
 * apontada por `dest`, lê seu conteudo como um `char *`, aloca
 * espaço para a string lida e guarda o endereço alocado nessa
 * região de memória.
 *
 * A forma de detectar um campo depende do tipo do arquivo a ser
 * lido, determinado por `ftype`, que pode ser `F_TYPE_CSV` ou
 * `F_TYPE_UNDELIM` --- delimitado por espaço em branco em vez
 * de usar caractere(s) delimitador(es) convencional(is).
 *
 * Para arquivos do tipo `F_TYPE_CSV`, uma vírgula/quebra de linha
 * delimitando um campo do tipo `T_STR` representa uma string nula
 * (isso implica que não é possível ler uma string vazia).
 * Já para arquivos do tipo `F_TYPE_UNDELIM`, strings devem aparecer
 * entre aspas duplas, e o valor nulo é `nulo` (sem aspas duplas).
 *
 * Os delimitadores aceitos para campos do tipo `T_STR` são
 * especificados por `delims`. Se for `NULL`, a string deverá
 * aparecer entre aspas duplas, não sendo possível ler strings
 * contendo aspas duplas.
 *
 * Espaços em branco são sempre permitidos antes dos campos.
 *
 * Retorna `false` se a leitura falhar.
 */
bool parse_field(FILE *f, enum f_type ftype, enum typeinfo info, void *dest);

/**
 * Lê o delimitador de registro CSV '\n' (opcionalmente precedido
 * por '\r'), preparando para realizar a leitura do próximo registro.
 *
 * Retorna `true` se a leitura suceder, indicando em `*eof` se o
 * final do arquivo foi alcançado (esse é o último registro).
 */
bool csv_next_record(FILE *f, bool *eof);

/**
 * Consome o espaço em branco presente na posição atual de `f`. Usado para
 * parsing de campos de arquivos com tipo `F_TYPE_UNDELIM`.
 */
void consume_whitespace(FILE *f);

#endif /* PARSE_H */
