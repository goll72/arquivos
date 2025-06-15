#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

#include <stdnoreturn.h>

#include "crud.h"
#include "defs.h"
#include "file.h"
#include "vset.h"
#include "error.h"

#include "util/hash.h"
#include "util/parse.h"


/**
 * Trechos de código marcados com `SYNC: tag` são trechos que devem ser
 * alterados concomitantemente, por dependerem de uma determinada ordem
 * de campos de um registro, por exemplo, em vez de abstrair essa
 * dependência de alguma forma.
 */

enum functionality {
    FUNC_CREATE_TABLE   =  1,
    FUNC_SELECT_STAR    =  2,
    FUNC_SELECT_WHERE   =  3,
    FUNC_DELETE_WHERE   =  4,
    FUNC_INSERT_INTO    =  5,
    FUNC_UPDATE_WHERE   =  6,
    FUNC_CREATE_INDEX   =  7,
    /* Equivalente às funcionalidades anteriores, porém com uso do arquivo de índice */
    FUNC_SELECT_WHERE_I =  8,
    FUNC_DELETE_WHERE_I =  9,
    FUNC_INSERT_INTO_I  = 10,
    FUNC_UPDATE_WHERE_I = 11,
};

/**
 * Aborta a execução do programa, imprimindo a mensagem `msg`.
 *
 * Embora isso não tenha sido especificado, observa-se que o
 * código de saída deve ser 0, caso contrário um caso de teste
 * no run.codes irá falhar.
 */
noreturn static void bail(char *msg)
{
    puts(msg);
    exit(0);
}

/**
 * Wrapper para a função `scanf` que aborta a execução
 * se não for possível ler a quantidade certa de campos.
 */
static void scanf_expect(int n, const char *restrict fmt, ...)
    __attribute__((format(scanf, 2, 3)));

static void scanf_expect(int n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if (vscanf(fmt, ap) != n)
        bail(E_PROCESSINGFILE);

    va_end(ap);
}

/* clang-format off */

/**
 * Lê um registro a partir de uma linha do arquivo `f`, cujo tipo
 * é o especificado por `ftype`. Armazena-o em `*rec`. Aborta a execução
 * se ocorrer algum erro.
 */
static void rec_parse(FILE *f, enum f_type ftype, f_data_rec_t *rec)
{
    rec->removed = REC_NOT_REMOVED;
    rec->size = DATA_REC_SIZE_AFTER_SIZE_FIELD;
    rec->next_removed_rec = -1;

    #define READ_COMMON(T, name)                                      \
        if (!parse_field(f, ftype, GET_TYPEINFO(T), &rec->name))      \
            bail(E_PROCESSINGFILE);

    #define FIXED_FIELD(T, name, ...) READ_COMMON(T, name)

    #define VAR_FIELD(T, name, ...) \
        READ_COMMON(T, name)        \
        rec->size += rec->name ? strlen(rec->name) + 2 : 0;

    // Ignora os campos de metadados (uma vez que esses campos não
    // são lidos do arquivo, mas sim inicializados pelo programa),
    // e lê os valores dos campos de tamanho fixo e variável,
    // em ordem, a partir do arquivo CSV, atualizando o tamanho do
    // registro de acordo com o tamanho de cada campo de tamanho
    // variável.
    //
    // NOTE: ao ler os campos de tamanho variável, somamos 2 ao
    // tamanho da string devido ao código do campo e ao delimitador,
    // que ocupam 1 byte cada
    #include "x/data.h"

    #undef READ_COMMON
}

/* clang-format on */

/**
 * Lê uma string (sem espaços) da `stdin`, abre um arquivo com
 * caminho dado por essa string, repassando o modo de abertura
 * `mode` para `fopen`. Lê o registro de cabeçalho presente no
 * arquivo e armazena-o em `*header`. Marca o arquivo como
 * inconsistente se for aberto para modificação (`mode` é
 * considerado um modo de abertura com modificação se conter
 * o caractere 'w' ou '+').
 *
 * Aborta a execução em caso de erro ou se o arquivo estiver
 * inconsistente.
 */
static FILE *file_open_from_stdin(f_header_t *header, const char *mode)
{
    char path[PATH_MAX];
    scanf_expect(1, "%s", path);

    FILE *f = fopen(path, mode);

    if (!f)
        bail(E_PROCESSINGFILE);

    if (!file_read_header(f, header) || header->status != STATUS_CONSISTENT)
        bail(E_PROCESSINGFILE);

    if (strchr(mode, 'w') || strchr(mode, '+')) {
        header->status = STATUS_INCONSISTENT;

        fseek(f, offsetof(PACKED(f_header_t), status), SEEK_SET);
        fwrite(&header->status, sizeof header->status, 1, f);

        // Retorna à posição inicial, após o header
        fseek(f, sizeof(PACKED(f_header_t)), SEEK_SET);
    }

    return f;
}

/**
 * Realiza a atualização do header e outras operações que devem ser feitas
 * ao final do execução (apenas quando o arquivo tiver sido modificado),
 * como imprimir o seu hash. Por fim, fecha o arquivo.
 */
static void file_cleanup_after_modify(FILE *f, f_header_t *header)
{
    // Marca o arquivo como "limpo"/consistente
    header->status = STATUS_CONSISTENT;

    // Escreve o header após realizar as modificações
    fseek(f, 0, SEEK_SET);
    file_write_header(f, header);

    // Imprime o hash do arquivo, equivalente à função binarioNaTela
    fseek(f, 0, SEEK_SET);
    printf("%lf\n", hash_file(f));

    fclose(f);
}

/**
 * Lê um vset (conjunto de valores de determinados campos do
 * registro do arquivo de dados) a partir da `stdin`, usando
 * os campos do arquivo de dados.
 */
static vset_t *vset_new_from_stdin(void)
{
    int n_conds;

    // Lê a quantidade de valores do vset
    scanf_expect(1, "%d", &n_conds);

    vset_t *vset = vset_new();

    for (int j = 0; j < n_conds; j++) {
        size_t offset;
        enum typeinfo info;
        uint8_t flags;

        char field_repr[64];

        // Lê o campo (representação na forma de string)
        scanf_expect(1, "%s", field_repr);

        // Recupera metadados sobre o campo que serão usados para
        // realizar a comparação: offset do campo na struct
        // `f_data_rec_t`, seu tipo (um `enum typeinfo`) e flags.
        if (!data_rec_typeinfo(field_repr, &offset, &info, &flags))
            bail(E_PROCESSINGFILE);

        // Irá guardar o valor referência para comparação do campo atual
        void *val = NULL;

        // Usado para que possamos fazer a leitura de uma string (`T_STR`)
        // alocada dinamicamente. Irá apontar para a string alocada
        // dinamicamente por `parse_field`, nesse caso.
        char *str;

        // Reserva espaço para guardar o valor que será lido (e posteriormente
        // usado na comparação), de acordo com o tipo do campo. Todos os valores
        // passados para o vset devem ser alocados dinamicamente. No entanto,
        // campos do tipo `T_STR` já são alocados dinamicamente pela função
        // `parse_field`, logo, só precisamos guardar o ponteiro temporariamente.
        switch (info) {
            case T_U32:
                val = malloc(sizeof(uint32_t));
                break;
            case T_FLT:
                val = malloc(sizeof(float));
                break;
            case T_STR:
                val = &str;
                break;
        }

        if (!parse_field(stdin, F_TYPE_UNDELIM, info, val))
            bail(E_PROCESSINGFILE);

        if (info == T_STR)
            val = str;

        // Adiciona um valor ao vset.
        //
        // Esse valor será comparado com o valor na posição `offset` da struct
        // `f_data_rec_t`, usando o tipo dado por `info` para realizar a comparação.
        vset_add_value(vset, offset, info, flags, val);
    }

    return vset;
}

/** Wrapper para a função `crud_delete` que aborta a execução em caso de erro */
void delete(FILE *f, f_header_t *header, f_data_rec_t *rec, void *data)
{
    if (!crud_delete(f, header, rec))
        bail(E_PROCESSINGFILE);
}

/** Wrapper para a função `crud_update` que aborta a execução em caso de erro */
void update(FILE *f, f_header_t *header, f_data_rec_t *rec, void *data)
{
    vset_t *patch = data;

    if (!crud_update(f, header, rec, patch))
        bail(E_PROCESSINGFILE);
}

int main(void)
{
    int func;
    f_header_t header;

    // Lê a funcionalidade da `stdin`
    scanf_expect(1, "%d", &func);

    switch (func) {
        case FUNC_CREATE_TABLE: {
            char csv_path[PATH_MAX];
            char bin_path[PATH_MAX];

            scanf_expect(2, "%s %s", csv_path, bin_path);

            FILE *csv_f = fopen(csv_path, "r");

            if (!csv_f)
                bail(E_PROCESSINGFILE);

            FILE *bin_f = fopen(bin_path, "wb+");

            if (!bin_f)
                bail(E_PROCESSINGFILE);

            file_init_header(&header);

            char *header_desc;
            size_t header_desc_len;

            /* clang-format off */

            #define X(T, name, ...)                                                       \
                if (!parse_field(csv_f, F_TYPE_CSV, T_STR, &header_desc) || !header_desc) \
                    bail(E_PROCESSINGFILE);                                               \
                                                                                          \
                header_desc_len = strlen(header_desc);                                    \
                                                                                          \
                if (header_desc_len > sizeof header.name##_desc)                          \
                    bail(E_PROCESSINGFILE);                                               \
                                                                                          \
                memcpy(&header.name##_desc, header_desc, header_desc_len);                \
                free(header_desc);                                                        \
                memset(&header.name##_desc[header_desc_len], '$', sizeof header.name##_desc - header_desc_len);

            #define FIXED_FIELD X
            #define VAR_FIELD   X

            // Lê os campos de descrição presentes no arquivo CSV e
            // os armazena nos campos de descrição do registro de cabeçalho,
            // abortando a execução do programa se a leitura falhar ou se o
            // campo exceder o tamanho máximo permitido. Se o campo lido do
            // arquivo CSV for menor que o campo correspondente de descrição
            // no arquivo binário, cifrões são adicionados ao final do campo.
            #include "x/data.h"

            #undef X

            /* clang-format on */

            if (!file_write_header(bin_f, &header))
                bail(E_PROCESSINGFILE);

            // Enquanto não tiver chegado ao final do arquivo CSV,
            // lê um registro e o escreve no arquivo de dados.
            while (true) {
                bool eof;

                if (!csv_next_record(csv_f, &eof))
                    bail(E_PROCESSINGFILE);

                if (eof)
                    break;

                f_data_rec_t rec;
                rec_parse(csv_f, F_TYPE_CSV, &rec);

                if (!file_write_data_rec(bin_f, &header, &rec))
                    bail(E_PROCESSINGFILE);

                rec_free_var_data_fields(&rec);

                header.n_valid_recs++;
            }

            header.next_byte_offset = ftell(bin_f);

            fclose(csv_f);
            file_cleanup_after_modify(bin_f, &header);

            break;
        }
        case FUNC_SELECT_STAR:
        case FUNC_SELECT_WHERE: {
            // Na funcionalidade 2 (`FUNC_SELECT_STAR`), apenas uma query é feita.
            int n_queries = 1;

            FILE *f = file_open_from_stdin(&header, "rb");

            if (func == FUNC_SELECT_WHERE)
                scanf_expect(1, "%d", &n_queries);

            for (int i = 0; i < n_queries; i++) {
                vset_t *filter;

                // No primeiro caso, o conjunto de condições do filtro (vset) será
                // o conjunto vazio, cujo (produto/E lógico) é (1/verdadeiro).
                // Dessa forma, todos os registros serão buscados.
                if (func == FUNC_SELECT_STAR)
                    filter = vset_new();
                else
                    filter = vset_new_from_stdin();

                f_data_rec_t rec = {};
                bool unique = false;
                bool no_matches = true;

                // Retorna à posição inicial, após o header
                fseek(f, sizeof(PACKED(f_header_t)), SEEK_SET);

                while (file_search_seq_next(f, &header, filter, &rec, &unique) != -1) {
                    file_print_data_rec(&header, &rec);

                    no_matches = false;

                    rec_free_var_data_fields(&rec);

                    // Se um dos campos buscados não permitir repetições
                    // e um registro foi encontrado, parar a busca
                    //
                    // NOTE: `unique` nunca será verdadeiro quando o conjunto
                    // de valores do filtro for vazio.
                    if (unique)
                        break;
                }

                vset_free(filter);

                if (no_matches) {
                    puts(E_NOREC);
                    printf("\n");
                }

                if (func == FUNC_SELECT_WHERE)
                    puts("**********");
            }

            fclose(f);

            break;
        }
        case FUNC_DELETE_WHERE: {
            FILE *f = file_open_from_stdin(&header, "rb+");

            int n_queries;
            scanf_expect(1, "%d", &n_queries);

            for (int i = 0; i < n_queries; i++) {
                vset_t *filter = vset_new_from_stdin();

                // Percorre o arquivo sequencialmente, chamando a função `delete`
                // para cada um dos registros válidos (não removidos) encontrados
                // que passam no filtro `filter`
                file_traverse_seq(f, &header, filter, delete, NULL);

                vset_free(filter);
            }

            file_cleanup_after_modify(f, &header);

            break;
        }
        case FUNC_INSERT_INTO: {
            FILE *f = file_open_from_stdin(&header, "rb+");

            int n_insertions;
            scanf_expect(1, "%d", &n_insertions);

            for (int i = 0; i < n_insertions; i++) {
                // Necessário para descartar a quebra de linha delimitando
                // a entrada, pois a função de parsing é estrita e iria rejeitar
                // a entrada se esse(s) caractere(s) não fosse(m) descartado(s)
                consume_whitespace(stdin);

                f_data_rec_t rec;
                rec_parse(stdin, F_TYPE_UNDELIM, &rec);

                // Insere o registro `rec` no arquivo, seguindo
                // as regras do algoritmo de reúso de espaço
                if (!crud_insert(f, &header, &rec))
                    bail(E_PROCESSINGFILE);

                rec_free_var_data_fields(&rec);
            }

            file_cleanup_after_modify(f, &header);

            break;
        }
        case FUNC_UPDATE_WHERE: {
            FILE *f = file_open_from_stdin(&header, "rb+");

            int n_queries;
            scanf_expect(1, "%d", &n_queries);

            for (int i = 0; i < n_queries; i++) {
                vset_t *filter = vset_new_from_stdin();
                vset_t *patch = vset_new_from_stdin();

                // Percorre o arquivo sequencialmente, chamando a função `update`
                // para cada um dos registros válidos (não removidos) encontrados
                // que passam no filtro `filter`
                file_traverse_seq(f, &header, filter, update, patch);

                vset_free(filter);
                vset_free(patch);
            }

            file_cleanup_after_modify(f, &header);

            break;
        }
    }
}
