#include "util/parse.h"

bool parse_read_field(
    FILE *f, enum typeinfo info, void *buf, const char *delims, bool quoted)
{
    if (!delims || delims[0] == '\0')
        return false;

    switch (info) {
        case T_U32: {
            break;
        }
        case T_FLT: {
            break;
        }
        case T_STR: {
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

bool csv_next_record(FILE *f)
{
    // Volta para o byte anterior, um byte delimitador
    // consumido pela função `parse_read_field`.
    fseek(f, -1, SEEK_CUR);

    int prev = fgetc(f);

    if (prev == '\n')
        return true;

    if (prev != '\r')
        return false;

    int c = fgetc(f);

    if (c != '\n')
        return false;

    return true;
}
