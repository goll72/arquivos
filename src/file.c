#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "file.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

#define FMT(x)                       \
    _Generic(x,                      \
        char *: "%2$.*1$s: %4$s\n",  \
        float: "%2$.*1$s: %4$.2f\n", \
        uint32_t: "%2$.*1$s: %4$" PRIu32 "\n")

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

    // Tamanho da string, considerando o
    // delimitador, porém ignorando o código
    size_t len = current - initial;

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

    *rem_size -= len;

    return data;
}

bool file_read_data_reg(FILE *f, const f_header_t *header, f_data_reg_t *reg)
{
#define X(T, name) FAIL_IF(fread(&reg->name, sizeof(T), 1, f) != 1)
#define Y(_, name)                                                      \
    reg->name = file_read_var_field(f, header->name##_code, &rem_size); \
    FAIL_IF(rem_size < 0)
#define Z(T, name)

    // Lê apenas os campos de tamanho fixo
    DATA_REG_FIELDS(X, X, Z)

    // Calcula o tamanho do restante do registro com base no tamanho
    // presente no arquivo, que inclui os campos de tamanho fixo após
    // o campo `size`. No entanto, queremos apenas o tamanho da parte
    // variável, logo, subtraímos o tamanho desses campos de tamanho
    // fixo que vêm após o campo `size` no registro.
    int64_t rem_size = reg->size - sizeof(PACKED(f_data_reg_t))
        + offsetof(PACKED(f_data_reg_t), size) + sizeof reg->size;

    // NOTE: um registro pode omitir campos de tamanho variável. Nesse
    // caso, a tentativa de leitura de um campo ausente irá "falhar",
    // porém esse campo irá receber o valor `NULL`, como esperado.
    DATA_REG_FIELDS(Z, Z, Y)

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

bool file_write_data_reg(FILE *f, const f_header_t *header, const f_data_reg_t *reg)
{
#define X(T, name) FAIL_IF(fwrite(&reg->name, sizeof(T), 1, f) != 1)
#define Y(_, name) FAIL_IF(!file_write_var_field(f, header->name##_code, reg->name))

    DATA_REG_FIELDS(X, X, Y)

#undef X
#undef Y

    return true;
}

void file_print_data_reg(const f_header_t *header, const f_data_reg_t *reg)
{
#define X(_, name)
#define Y(_, name)                                                               \
    printf(FMT(reg->name), (int)sizeof header->name##_desc, header->name##_desc, \
        (int)sizeof reg->name, reg->name);

    // Ignora os campos de metadados e imprime
    // os campos de dados, usando as descrições
    // presentes no cabeçalho
    DATA_REG_FIELDS(X, Y, Y)

#undef X
#undef Y
}
