#include <string.h>
#include <stdlib.h>

#include "file.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

bool file_init(FILE *f)
{
#define X(_, name, default) .name = default,

    static const f_header_t initial_header = { HEADER_REG_FIELDS(X) };

#undef X

    return file_write_header(f, &initial_header);
}

bool file_read_header(FILE *f, f_header_t *header)
{
#define X(T, name, _) FAIL_IF(fread(&header->name, sizeof(T), 1, f) != 1)

    HEADER_REG_FIELDS(X)

#undef X

    return true;
}

bool file_write_header(FILE *f, const f_header_t *header)
{
#define X(T, name, _) FAIL_IF(fwrite(&header->name, sizeof(T), 1, f) != 1)

    HEADER_REG_FIELDS(X)

#undef X

    return true;
}

/**
 * Lê um campo de tamanho variável (string delimitada por '|')
 * a partir da posição atual do arquivo `f` e retorna o valor
 * lido em uma string alocada dinamicamente.
 *
 * Retorna `NULL` caso a leitura falhe.
 */
static char *file_read_var_field(FILE *f)
{
    long initial = ftell(f);
    int c = 0;

    do {
        if (c == EOF)
            return NULL;

        c = fgetc(f);
    } while (c != '|');

    long current = ftell(f);

    // Tamanho da string, contando com o delimitador
    size_t len = current - initial + 1;
    char *data = malloc(len);

    if (!data)
        return NULL;

    fseek(f, initial, SEEK_SET);

    // Também queremos ler o delimitador, mesmo
    // que ele seja imediatamente sobrescrito
    fread(data, 1, len, f);
    data[len - 1] = '\0';

    return data;
}

bool file_read_data_reg(FILE *f, f_data_reg_t *reg)
{
#define X(T, name) FAIL_IF(fread(&reg->name, sizeof(T), 1, f) != 1)
#define Y(_, name)                      \
    reg->name = file_read_var_field(f); \
    FAIL_IF(!reg->name)

    DATA_REG_FIELDS(X, Y)

#undef X
#undef Y

    return true;
}

/**
 * Escreve a string `data` na posição atual do arquivo `f`,
 * como um campo de tamanho variável, delimitado por '|'.
 */
static bool file_write_var_field(FILE *f, char *data)
{
    size_t len = strlen(data) + 1;

    data[len - 1] = '|';
    bool status = fwrite(data, 1, len, f) == len;
    data[len - 1] = '\0';

    return status;
}

bool file_write_data_reg(FILE *f, const f_data_reg_t *reg)
{
#define X(T, name) FAIL_IF(fwrite(&reg->name, sizeof(T), 1, f) != 1)
#define Y(_, name) FAIL_IF(!file_write_var_field(f, reg->name))

    DATA_REG_FIELDS(X, Y)

#undef X
#undef Y

    return true;
}
