#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "util/parse.h"

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

    if (*len == *cap) {
        *cap *= 2;
        result = realloc(result, *cap);
    }

    result[*len] = c;
    *len += 1;

    return result;
}

bool parse_read_field(FILE *f, enum typeinfo info, void *dest, const char *delims)
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

            if (!is_null) {
                int ret = fscanf(f, "%" SCNu32, &result);

                if (ret != 1)
                    return false;
            }

            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        case T_FLT: {
            float result = -1;

            if (!is_null) {
                int ret = fscanf(f, "%f", &result);

                if (ret != 1)
                    return false;
            }

            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        case T_STR: {
            char *result = NULL;
            int c;

            if (!delims) {
                c = fgetc(f);

                if (c != '"') {
                    char buf[5] = {};
                    char *ptr = buf;

                    // Lê no máximo 4 caracteres para verificar se a string lida corresponde a um valor "null"
                    do {
                        *ptr++ = tolower(c);
                        c = fgetc(f);
                    } while (isalpha(c) && ptr != &buf[sizeof buf - 1]);

                    *ptr = '\0';

                    if (!strcmp(buf, "nil") || !strcmp(buf, "null") || !strcmp(buf, "nulo")) {
                        if (dest)
                            memcpy(dest, &result, sizeof result);

                        break;
                    }

                    return false;
                }
            }

            size_t cap = 8;
            size_t len = 0;

            if (dest)
                result = malloc(cap);

            while (true) {
                c = fgetc(f);

                if ((!delims && c == '"') || (delims && strchr(delims, c))) {
                    // Não é possível ler strings vazias usando delimitadores,
                    // essas strings são convertidas para `NULL`
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

bool csv_read_field(FILE *f, enum typeinfo info, void *dest)
{
    bool status = parse_read_field(f, info, dest, ",\r\n");

    if (!status)
        return false;

    int c;

    // Nesse caso, o delimitador foi lido em `parse_read_field`,
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

bool csv_next_record(FILE *f, bool *eof)
{
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
