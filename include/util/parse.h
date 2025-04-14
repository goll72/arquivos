#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdbool.h>

#include "typeinfo.h"

/**
 * Funções para lidar com parsing de strings.
 */

/**
 * Lê o campo com tipo especificado por `info`, armazenando o
 * resultado na região de memória apontada por `dest`. Se
 * `dest` for `NULL`, a leitura é realizada, porém o resultado
 * não é armazenado.
 *
 * Caso `info` seja `T_STR`, dereferencia a região de memória
 * apontada por `dest`, lê seu conteudo como um `char *`, aloca
 * espaço para a string lida e guarda o endereço alocado nessa
 * região de memória.
 *
 * `delims` é uma string contendo os valores de delimitador
 * aceitos. Se `delims` tiver um valor válido, porém a leitura
 * falhar, todos os bytes até o próximo delimitador serão lidos.
 *
 * Se `quoted` for `true`, apenas irá aceitar strings com
 * aspas duplas, retirando as aspas duplas e permitindo
 * ocorrência dos delimitadores dentro das aspas duplas.
 * Também permitirá espaços em branco antes da string.
 *
 * Espaços em branco são sempre permitidos antes dos campos
 * de tipo `T_U32` e `T_FLT`.
 *
 * Valores ausentes ("nulos") são permitidos. Campos do tipo
 * `T_U32` e `T_FLT` serão inicializados com `UINT_MAX`
 * (equivalente a `(uint32_t) -1`) e `-1.f`, respectivamente,
 * nesse caso. Não é possível ler uma string vazia, as sequências
 * <delim> (se `!quoted`) e ["] ["] <delim> (se `quoted`) são
 * ambas interpretadas como `NULL`.
 *
 * Retorna `false` se a leitura falhar, se `delims` for `NULL`
 * ou se `delims` for vazia.
 */
bool parse_read_field(FILE *f, enum typeinfo info, void *dest,
                      const char *delims, bool quoted);

/**
 * Funções para lidar com arquivos CSV. Essas funções asssumem
 * que os arquivos são "seekable" (`fseek` pode ser usado), ao
 * contrário das funções acima.
 */

/**
 * Lê o campo atual do arquivo CSV `f`.
 *
 * vd. `parse_read_field`.
 */
bool csv_read_field(FILE *f, enum typeinfo info, void *buf);

/**
 * Lê o delimitador de registro CSV '\n' (opcionalmente precedido
 * por '\r'), preparando para realizar a leitura do próximo registro.
 *
 * Retorna `true` se a leitura suceder, indicando em `*eof` se o
 * final do arquivo foi alcançado (esse é o último registro).
 */
bool csv_next_record(FILE *f, bool *eof);

#endif /* PARSE_H */
