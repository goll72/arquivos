#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "defs.h"
#include "file.h"
#include "vset.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

/** Define o valor nulo de um campo de acordo com seu tipo */
#define NULL_VALUE(x) _Generic(x, char *: NULL, float: -1, uint32_t: -1)

/**
 * Define como um campo deve ser formatado
 * ao ser impresso, de acordo com seu tipo
 */
#define FMT(x)                 \
    _Generic(x,                \
        char *: "%.*s: %s\n",  \
        float: "%.*s: %.2f\n", \
        uint32_t: "%.*s: %" PRIu32 "\n")

/**
 * Calcula o tamanho de uma string armazenada em um campo de
 * tamanho fixo no arquivo, usando para isso o fato de que a
 * primeira ocorrência do caractere '$' (cifrão/lixo) indica
 * o término da string.
 *
 * Se o caractere '$' não estiver presente na string (`memchr`
 * irá retornar `NULL`), a string está ocupando todo o espaço
 * disponível no campo.
 */
#define LEN_FIXED_STR(s) \
    (((char *)memchr(s, '$', sizeof s) ?: &s[sizeof s]) - s)

/* clang-format off */

void file_init_header(f_header_t *header)
{
    #define HEADER_FIELD(T, name, default) .name = default,

    // Inicializa `initial_header` com os valores padrão definidos em "defs.h".
    static const f_header_t initial_header = {
        #include "x/header.h"
    };

    memcpy(header, &initial_header, sizeof initial_header);
}

bool file_read_header(FILE *f, f_header_t *header)
{
    #define HEADER_FIELD(T, name, ...) FAIL_IF(fread(&header->name, sizeof(T), 1, f) != 1)

    // Todos os campos do cabeçalho têm tamanho fixo,
    // logo não precisamos de lógica adicional além
    // de ler os valores, campo a campo
    #include "x/header.h"

    return true;
}

bool file_write_header(FILE *f, const f_header_t *header)
{
    #define HEADER_FIELD(T, name, ...) FAIL_IF(fwrite(&header->name, sizeof(T), 1, f) != 1)

    // Analogamente, para escrita (vd. `file_read_header`)
    #include "x/header.h"

    return true;
}

/* clang-format on */

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
    // Se o tamanho restante é 0, o registro atual acabou de ser lido
    // e quaisquer campos de tamanho variável que ainda não foram lidos
    // foram omitidos (são ausentes/nulos).
    //
    // Isso não indica, necessariamente, um erro; apenas que algum
    // campo opcional não está presente.
    if (*rem_size == 0)
        return NULL;

    // A posição inicial é guardada para que possamos voltar
    // para trás em caso de erro e para que possamos calcular
    // o tamanho da string lida quando encontrarmos o delimitador.
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
    // NOTE: o byte na posição atual do arquivo não foi lido,
    // uma vez que a posição atual é incrementada após a leitura
    //
    // Ou seja, esse byte não faz parte do campo, logo, devemos subtrair 1
    int64_t len = current - initial - 1;

    // Se o tamanho da string lida é maior que o tamanho restante do
    // registro, o registro está errado e a leitura deve falhar
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
    if (fread(data, 1, len, f) != len) {
        free(data);
        return NULL;
    }

    data[len - 1] = '\0';

    // Inclui o código no cálculo do tamanho restante do registro
    *rem_size -= len + 1;

    return data;
}

/* clang-format off */

bool file_read_data_rec(FILE *f, const f_header_t *header, f_data_rec_t *rec)
{
    #define X(T, name, ...) FAIL_IF(fread(&rec->name, sizeof(T), 1, f) != 1)
    #define Y(T, name, ...)                                                 \
        rec->name = file_read_var_field(f, header->name##_code, &rem_size); \
        FAIL_IF(rem_size < 0)

    #define METADATA_FIELD X

    // Lê apenas os campos de metadados: se o registro for removido, pula
    // para o final do registro, sem ler os dados
    #include "x/data.h"

    if (rec->removed != REC_REMOVED && rec->removed != REC_NOT_REMOVED)
        return false;

    if (rec->removed == REC_REMOVED) {
        fseek(f, rec->size - sizeof rec->next_removed_rec, SEEK_CUR);
        return true;
    }

    // Lê apenas os campos de dados
    #define FIXED_FIELD X

    // Calcula o tamanho do restante do registro com base no tamanho
    // presente no arquivo, que inclui os campos de tamanho fixo após
    // o campo `size`. No entanto, queremos apenas o tamanho da parte
    // variável, logo, subtraímos o tamanho desses campos de tamanho
    // fixo que vêm após o campo `size` no registro.
    int64_t rem_size = rec->size - DATA_REC_SIZE_AFTER_SIZE_FIELD;

    // NOTE: um registro pode omitir campos de tamanho variável. Nesse
    // caso, a tentativa de leitura de um campo ausente irá "falhar",
    // porém esse campo irá receber o valor `NULL`, como esperado.
    #define VAR_FIELD Y

    #include "x/data.h"

    #undef X
    #undef Y

    // Verifica que o final do registro possui lixo válido
    while (rem_size > 0) {
        if (fgetc(f) != '$')
            return false;

        rem_size--;
    }

    return rem_size == 0;
}

/* clang-format on */

/**
 * Escreve a string `data` na posição atual do arquivo `f`, como um campo de
 * tamanho variável, precedido pelo seu código, `code`, e delimitado por '|'.
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

/* clang-format off */

bool file_write_data_rec(FILE *f, const f_header_t *header, const f_data_rec_t *rec)
{
    #define X(T, name, ...) FAIL_IF(fwrite(&rec->name, sizeof(T), 1, f) != 1)
    #define Y(T, name, ...) FAIL_IF(!file_write_var_field(f, header->name##_code, rec->name))

    #define METADATA_FIELD X
    #define FIXED_FIELD    X
    #define VAR_FIELD      Y

    // Escreve os campos de tamanho fixo diretamente, usando `fwrite` (X),
    // e os campos de tamanho variável usando a função `file_write_var_field`,
    // passando para essa função o código do campo correspendente presente
    // no registro de cabeçalho (Y).
    #include "x/data.h"

    #undef X
    #undef Y

    return true;
}

int64_t file_search_seq_next(FILE *f, const f_header_t *header, vset_t *vset, f_data_rec_t *rec, bool *unique)
{
    long current = ftell(f);

    while (current < header->next_byte_offset) {
        current = ftell(f);

        if (!file_read_data_rec(f, header, rec))
            return -1;

        if (rec->removed == REC_REMOVED)
            continue;

        if (vset_match_against(vset, rec, unique))
            return current;
    }

    return -1;
}

void file_print_data_rec(const f_header_t *header, const f_data_rec_t *rec)
{
    #define HEADER_DESC_ARGS(name) (int)LEN_FIXED_STR(header->name##_desc), header->name##_desc

    #define DATA_FIELD(name)                                       \
        if (rec->name == NULL_VALUE(rec->name))                    \
            printf("%.*s: NADA CONSTA\n", HEADER_DESC_ARGS(name)); \
        else                                                       \
            printf(FMT(rec->name), HEADER_DESC_ARGS(name), rec->name);

    // Imprime os campos de dados, usando as descrições para
    // cada campo que estão presentes no registro de cabeçalho
    #include "x/data-print.h"

    putc('\n', stdout);

    #undef HEADER_DESC_ARGS
}
