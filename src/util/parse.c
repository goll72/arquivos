#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "util/parse.h"

#define IS_NULL_STR_VALUE(buf) !strcmp(buf, "nulo")

/**
 * Adiciona o caractere `c` ao buffer `result` (que possui tamanho `*len`, ou seja,
 * apresenta `*len` bytes ocupados, e capacidade `*cap`), realocando-o conforme
 * necessário. O caractere delimitador '\0' deve ser adicionado manualmente ao final
 * da string.
 */
static inline char *append_realloc(char *result, size_t *len, size_t *cap, char c)
{
    if (!result)
        return NULL;

    // Se não houver mais espaço na string, realoca usando a estratégia de
    // dobrar o tamanho (complexidade de tempo amortizada linear)
    if (*len == *cap) {
        *cap *= 2;
        result = realloc(result, *cap);
    }

    result[*len] = c;
    *len += 1;

    return result;
}

/**
 * Os delimitadores aceitos para campos do tipo `T_STR` são
 * especificados por `delims`. Se for `NULL`, a string deverá
 * aparecer entre aspas duplas, não sendo possível ler strings
 * contendo aspas duplas.
 *
 * Valores ausentes ("nulos") são permitidos. Os delimitadores
 * passados em `delims` são usados para verificar se os campos
 * estão presentes. Para essa verificação, '\r' e '\n' também
 * são considerados delimitadores. Note que delimitadores só
 * são "lidos" (consumidos) ao ler campos com valor ausente.
 *
 * Campos do tipo `T_U32` e `T_FLT` serão inicializados com
 * `UINT_MAX` (equivalente a `(uint32_t) -1`) e `-1.f`,
 * respectivamente, nesse caso.
 */
static bool parse_field_by_delims(FILE *f, enum typeinfo info, void *dest, const char *delims)
{
    int c;

    // Devemos ler alguns tipos de espaço em branco (' ' e '\t') antes de seguir
    // para o parsing para que possamos verificar se um campo é nulo ou não.
    do {
        c = fgetc(f);
    } while (c == ' ' || c == '\t');

    // Se um campo for nulo, não devemos chamar a função `fscanf`, uma vez que
    // essa função "consome" (lê e discarta) todos os caracteres de espaço em branco
    // que ocorram antes do valor do campo em si, incluindo '\r' e '\n', caracteres
    // esses usados como delimitador de registro em arquivos CSV.
    bool is_null = (c == '\n' || c == '\r') || (delims && strchr(delims, c));

    ungetc(c, f);

    switch (info) {
        case T_U32: {
            uint32_t result = -1;

            // Lê o valor, se não for nulo
            if (!is_null) {
                int ret = fscanf(f, "%" SCNu32, &result);

                if (ret != 1)
                    return false;
            }

            // Copia o valor lido para o buffer passado pelo usuário
            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        case T_FLT: {
            float result = -1;

            // Lê o valor, se não for nulo
            if (!is_null) {
                int ret = fscanf(f, "%f", &result);

                if (ret != 1)
                    return false;
            }

            // Copia o valor lido para o buffer passado pelo usuário
            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        case T_STR: {
            char *result = NULL;
            int c;

            if (!delims) {
                c = fgetc(f);

                // Se delimitadores de campo não foram especificados (o que implica na string tendo
                // que ser delimitada por aspas duplas --- "") e o primeiro caractere (diferente de espaço)
                // lido não for '"', a única possibilidade para a string lida é o valor nulo
                // (ou algum valor inválido/lixo, que nessa implementação é ignorado)
                if (c != '"') {
                    char buf[5] = {};
                    char *ptr = buf;

                    // Lê no máximo 4 caracteres para verificar se
                    // a string lida corresponde a um valor "nulo"
                    do {
                        *ptr++ = tolower(c);
                        c = fgetc(f);
                    } while (isalpha(c) && ptr != &buf[sizeof buf - 1]);

                    *ptr = '\0';

                    if (IS_NULL_STR_VALUE(buf)) {
                        // Copia o valor `NULL` para o buffer passado pelo usuário
                        if (dest)
                            memcpy(dest, &result, sizeof result);

                        break;
                    }

                    return false;
                }
            }

            size_t cap = 8;
            size_t len = 0;

            // Se o usuário passar um buffer válido, devemos alocar espaço para ler a string
            // Caso contrário, não precisamos armazenar a string lida
            if (dest)
                result = malloc(cap);

            // Lê o próximo caractere da string, até que seja
            // encontrado o '"' ou o delimitador de campo
            while (true) {
                c = fgetc(f);

                if ((!delims && c == '"') || (delims && strchr(delims, c))) {
                    // Não é possível ler strings vazias usando delimitadores
                    // de campo, essas strings são convertidas para `NULL`
                    if (delims && len == 0) {
                        free(result);
                        result = NULL;
                    }

                    result = append_realloc(result, &len, &cap, '\0');
                    break;
                }

                result = append_realloc(result, &len, &cap, c);
            }

            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        default:
            return false;
    }

    return true;
}

/**
 * Lê o campo atual do arquivo CSV `f`. Assume que o arquivo é
 * "seekable", ou seja, `fseek` pode ser usado, ao contrário de
 * outras funções.
 */
static bool csv_read_field(FILE *f, enum typeinfo info, void *dest)
{
    bool status = parse_field_by_delims(f, info, dest, ",\r\n");

    if (!status)
        return false;

    int c;

    // Nesse caso, o delimitador foi lido em `parse_field_by_delims`,
    // mas devemos lê-lo novamente.
    if (info == T_STR)
        fseek(f, -1, SEEK_CUR);

    do {
        c = fgetc(f);

        // A função `csv_next_record` fará uma checagem mais rigorosa
        if (c == '\r' || c == '\n') {
            ungetc(c, f);
            return true;
        }
    } while (isspace(c));

    return c == ',';
}

/** Lê um campo de um arquivo cujos valores são separados por espaço. */
static bool undelim_read_field(FILE *f, enum typeinfo info, void *dest)
{
    return parse_field_by_delims(f, info, dest, NULL);
}

bool parse_field(FILE *f, enum f_type ftype, enum typeinfo info, void *dest)
{
    switch (ftype) {
        case F_TYPE_CSV:
            return csv_read_field(f, info, dest);
        case F_TYPE_UNDELIM:
            return undelim_read_field(f, info, dest);
        default:
            return false;
    }
}

bool csv_next_record(FILE *f, bool *eof)
{
    // Precisamos guardar os últimos dois caracteres encontrados em `f`
    // para verificar se uma sequência válida de fim de linha foi encontrada,
    // visto que "\r\n" é uma sequência válida
    int c = ' ';
    int prev = c;

    bool valid = true;

    while (isspace(c)) {
        if (prev == '\r' || c == '\n') {
            // Tanto "\r\n" quanto '\n' são aceitos como sequências válidas de
            // delimitadores. '\r' seguido por outro caractere não é válido.
            valid = c == '\n';

            c = fgetc(f);
            break;
        }

        prev = c;
        c = fgetc(f);
    }

    ungetc(c, f);
    *eof = c == EOF;

    return valid;
}
