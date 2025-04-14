#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>

#include "util/parse.h"

static void read_until_delim(FILE *f, const char *delims)
{
    int c = 0;

    do {
        c = fgetc(f);
    } while (c != EOF && !strchr(delims, c));
}

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

bool parse_read_field(
    FILE *f, enum typeinfo info, void *dest, const char *delims, bool quoted)
{
    if (!delims || delims[0] == '\0')
        return false;

    switch (info) {
        case T_U32: {
            uint32_t result = -1;
            int c = fgetc(f);

            if (!strchr(delims, c)) {
                ungetc(c, f);

                int ret = fscanf(f, "%" SCNu32, &result);

                c = fgetc(f);

                if (ret != 1 || c == EOF || !strchr(delims, c)) {
                    read_until_delim(f, delims);
                    return false;
                }
            }

            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        case T_FLT: {
            float result = -1;
            int c = fgetc(f);

            if (!strchr(delims, c)) {
                ungetc(c, f);

                int ret = fscanf(f, "%f", &result);

                c = fgetc(f);

                if (ret != 1 || c == EOF || !strchr(delims, c)) {
                    read_until_delim(f, delims);
                    return false;
                }
            }

            if (dest)
                memcpy(dest, &result, sizeof result);

            break;
        }
        case T_STR: {
            char *result = NULL;
            int c = fgetc(f);

            if (strchr(delims, c)) {
                if (quoted)
                    return false;

                if (dest)
                    memcpy(dest, &result, sizeof result);

                return true;
            }

            if (quoted) {
                while (isspace(c))
                    c = fgetc(f);

                if (c != '"')
                    return false;
            } else {
                ungetc(c, f);
            }

            size_t cap = 8;
            size_t len = 0;

            int prev;

            if (dest)
                result = malloc(cap);

            while (true) {
                prev = c;

                c = fgetc(f);

                if (c == EOF) {
                    free(result);
                    return false;
                }

                if (strchr(delims, c)) {
                    if (!quoted) {
                        result = append_realloc(result, &len, &cap, '\0');
                        break;
                    } else if (quoted && prev == '"') {
                        len--;

                        if (len == 0) {
                            free(result);
                            result = NULL;

                            if (dest)
                                memcpy(dest, &result, sizeof result);

                            return true;
                        }

                        result = append_realloc(result, &len, &cap, '\0');
                        break;
                    }
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
    return parse_read_field(f, info, buf, ",\r\n", false);
}

bool csv_next_record(FILE *f, bool *eof)
{
    // Volta para o byte anterior, um byte delimitador
    // consumido pela função `parse_read_field`.
    fseek(f, -1, SEEK_CUR);

    int prev = fgetc(f);

    if (prev != '\n') {
        if (prev != '\r')
            return false;

        int c = fgetc(f);

        if (c != '\n')
            return false;
    }

    int c = fgetc(f);
    *eof = c == EOF;
    ungetc(c, f);

    return true;
}
