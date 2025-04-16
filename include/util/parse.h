#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdbool.h>

#include "typeinfo.h"

/** Funções para lidar com parsing de strings. */

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
 * Os delimitadores aceitos para campos do tipo `T_STR` são
 * especificados por `delims`. Se for `NULL`, a string deverá
 * aparecer entre aspas duplas, não sendo possível ler strings
 * contendo aspas duplas.
 *
 * Espaços em branco são sempre permitidos antes dos campos.
 * Não é possível ler uma string contendo apenas espaços se
 * `delims` não for `NULL`.
 *
 * Valores ausentes ("nulos") são permitidos. Os delimitadores
 * passados em `delims` são usados para verificar se os campos
 * estão presentes. Para essa verificação, '\r' e '\n' também
 * são considerados delimitadores. Note que delimitadores só
 * são "lidos" (consumidos) ao ler campos com valor ausente.
 *
 * Campos do tipo `T_U32` e `T_FLT` serão inicializados com
 * `UINT_MAX` (equivalente a `(uint32_t) -1`) e `-1.f`,
 * respectivamente, nesse caso. Para ler uma string vazia, é
 * necessário que `delims` seja `NULL`. Além disso, para ler
 * uma string "nula" se `delims` for `NULL`, os valores `nil`,
 * `null` ou `nulo` podem ser usados, sem aspas (maiúsculo ou
 * minúsculo).
 *
 * Retorna `false` se a leitura falhar.
 */
bool parse_read_field(FILE *f, enum typeinfo info, void *dest, const char *delims);

/**
 * Funções para lidar com arquivos CSV. Essas funções asssumem
 * que os arquivos são "seekable" (`ftell` e `fseek` podem ser
 * usados), ao contrário das funções acima.
 */

/**
 * Lê o campo atual do arquivo CSV `f`.
 *
 * vd. `parse_read_field`.
 */
bool csv_read_field(FILE *f, enum typeinfo info, void *dest);

/**
 * Lê o delimitador de registro CSV '\n' (opcionalmente precedido
 * por '\r'), preparando para realizar a leitura do próximo registro.
 *
 * Retorna `true` se a leitura suceder, indicando em `*eof` se o
 * final do arquivo foi alcançado (esse é o último registro).
 */
bool csv_next_record(FILE *f, bool *eof);

#endif /* PARSE_H */
