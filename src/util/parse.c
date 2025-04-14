#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "util/parse.h"

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

    do {
        c = fgetc(f);
    } while (c == ' ' || c == '\t');

    bool is_null = c == '\n' || (delims && strchr(delims, c));

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

                if (c != '"')
                    return false;
            }

            size_t cap = 8;
            size_t len = 0;

            if (dest)
                result = malloc(cap);

            while (true) {
                c = fgetc(f);

                if ((!delims && c == '"') || (delims && strchr(delims, c))) {
                    if (len == 0) {
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

bool csv_read_field(FILE *f, enum typeinfo info, void *buf)
{
    bool status = parse_read_field(f, info, buf, ",\r\n");

    if (!status)
        return false;

    int c;

    // Nesse caso, o delimitador foi lido em `parse_read_field`,
    // mas devemos lê-lo novamente.
    if (info == T_STR)
        fseek(f, -1, SEEK_CUR);

    do {
        c = fgetc(f);

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
