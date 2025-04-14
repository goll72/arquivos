#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "file.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

#define NULL_VALUE(x) _Generic(x, char *: NULL, float: -1, uint32_t: -1)

#define FMT(x)                 \
    _Generic(x,                \
        char *: "%.*s: %s\n",  \
        float: "%.*s: %.2f\n", \
        uint32_t: "%.*s: %" PRIu32 "\n")

void file_init_header(f_header_t *header)
{
#define X(T, name, default) .name = default,

    static const f_header_t initial_header = { HEADER_REC_FIELDS(X) };

#undef X

    memcpy(header, &initial_header, sizeof initial_header);
}

bool file_read_header(FILE *f, f_header_t *header)
{
#define X(T, name, ...) FAIL_IF(fread(&header->name, sizeof(T), 1, f) != 1)

    HEADER_REC_FIELDS(X)

#undef X

    return true;
}

bool file_write_header(FILE *f, const f_header_t *header)
{
#define X(T, name, ...) FAIL_IF(fwrite(&header->name, sizeof(T), 1, f) != 1)

    HEADER_REC_FIELDS(X)

#undef X

    return true;
}

/**
 * Lê um campo de tamanho variável (uma string delimitada por '|', onde o
 * primeiro byte deve ser o código `code` do campo) a partir da posição atual
 * do arquivo `f` e retorna o valor lido em uma string alocada dinamicamente.
 *
 * O parâmetro `rem_size` deve apontar para um `uint64_t` contendo o tamanho
 * restante do registro (a partir da posição atual no arquivo) e é atualizado
 * para o tamanho restante após a leitura, ou -1 se a leitura aparentemente
 * sucedeu, porém foram lidos mais bytes que o tamanho restante do registro.
 *
 * Reposiciona o arquivo para a posição anterior à chamada da função se o
 * código lido do arquivo for diferente de `code`.
 *
 * Retorna `NULL` se `*rem_size` for 0, se a leitura falhar, se o código
 * lido do arquivo for diferente de `code` ou se a quantidade de bytes
 * lido for maior que o tamanho do registro.
 */
static char *file_read_var_field(FILE *f, uint8_t code, int64_t *rem_size)
{
    if (*rem_size == 0)
        return NULL;

    long initial = ftell(f);
    int c = fgetc(f);

    if (c != code) {
        fseek(f, initial, SEEK_SET);
        return NULL;
    }

    do {
        c = fgetc(f);

        if (c == EOF)
            return NULL;
    } while (c != '|');

    long current = ftell(f);

    // Tamanho da string, incluindo o delimitador e ignorando o código
    //
    // NOTE: o byte na posição atual do arquivo não foi lido
    // (não faz parte do campo), logo, devemos subtrair 1
    int64_t len = current - initial - 1;

    if (len > *rem_size) {
        *rem_size = -1;
        return NULL;
    }

    char *data = malloc(len);

    if (!data)
        return NULL;

    // Pula o código
    fseek(f, initial + 1, SEEK_SET);

    // Também queremos ler o delimitador, mesmo
    // que ele seja imediatamente sobrescrito
    fread(data, 1, len, f);
    data[len - 1] = '\0';

    // Inclui o código no cálculo do tamanho restante do registro
    *rem_size -= len + 1;

    return data;
}

bool file_read_data_rec(FILE *f, const f_header_t *header, f_data_rec_t *rec)
{
#define X(T, name, ...) FAIL_IF(fread(&rec->name, sizeof(T), 1, f) != 1)
#define Y(T, name, ...)                                                 \
    rec->name = file_read_var_field(f, header->name##_code, &rem_size); \
    FAIL_IF(rem_size < 0)
#define Z(...)

    // Lê apenas os campos de tamanho fixo
    DATA_REC_FIELDS(X, X, Z)

    // Calcula o tamanho do restante do registro com base no tamanho
    // presente no arquivo, que inclui os campos de tamanho fixo após
    // o campo `size`. No entanto, queremos apenas o tamanho da parte
    // variável, logo, subtraímos o tamanho desses campos de tamanho
    // fixo que vêm após o campo `size` no registro.
    int64_t rem_size = rec->size - DATA_REC_SIZE_AFTER_SIZE_FIELD;

    // NOTE: um registro pode omitir campos de tamanho variável. Nesse
    // caso, a tentativa de leitura de um campo ausente irá "falhar",
    // porém esse campo irá receber o valor `NULL`, como esperado.
    DATA_REC_FIELDS(Z, Z, Y)

#undef X
#undef Y
#undef Z

    return rem_size == 0;
}

/**
 * Escreve a string `data` na posição atual do arquivo `f`,
 * como um campo de tamanho variável, delimitado por '|'.
 */
static bool file_write_var_field(FILE *f, uint8_t code, char *data)
{
    if (!data)
        return true;

    size_t len = strlen(data) + 1;

    FAIL_IF(fwrite(&code, 1, 1, f) != 1)

    data[len - 1] = '|';
    bool status = fwrite(data, 1, len, f) == len;
    data[len - 1] = '\0';

    return status;
}

bool file_write_data_rec(FILE *f, const f_header_t *header, const f_data_rec_t *rec)
{
#define X(T, name, ...) FAIL_IF(fwrite(&rec->name, sizeof(T), 1, f) != 1)
#define Y(T, name, ...) FAIL_IF(!file_write_var_field(f, header->name##_code, rec->name))

    DATA_REC_FIELDS(X, X, Y)

#undef X
#undef Y

    return true;
}

void file_print_data_rec(const f_header_t *header, const f_data_rec_t *rec)
{
#define HEADER_DESC_ARGS(name) (int)sizeof header->name##_desc, header->name##_desc

#define X(name)                                                \
    if (rec->name == NULL_VALUE(rec->name))                    \
        printf("%.*s: NADA CONSTA\n", HEADER_DESC_ARGS(name)); \
    else                                                       \
        printf(FMT(rec->name), HEADER_DESC_ARGS(name), rec->name);

    // Imprime os campos de dados, usando as descrições presentes no cabeçalho
    DATA_REC_PRINT_FIELDS(X)

#undef X

#undef HEADER_DESC_ARGS
}
