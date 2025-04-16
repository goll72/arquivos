#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <stdnoreturn.h>

#include "defs.h"
#include "file.h"
#include "query.h"
#include "error.h"

#include "util/hash.h"
#include "util/parse.h"


enum functionality {
    FUNC_CREATE_TABLE = 1,
    FUNC_SELECT_STAR = 2,
    FUNC_SELECT_WHERE = 3,
};

/* clang-format off */

/**
 * Apaga os campos de tamanho variável do registro `rec`.
 */
static void free_var_data_fields(f_data_rec_t *rec)
{
    #define VAR_FIELD(T, name, ...) free(rec->name);

    #include "x/data.h"
}

/* clang-format on */

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
 * Idêntica à função `file_read_data_rec`, porém verifica se
 * o resultado da tentativa de leitura é coerente. Se não for,
 * aborta a execução do programa.
 *
 * Retorna `true` se ainda não tiver chegado ao final do arquivo.
 */
static bool file_read_data_rec_or_bail(
    FILE *f, const f_header_t *header, f_data_rec_t *rec)
{

    if (!file_read_data_rec(f, header, rec)) {
        free_var_data_fields(rec);

        long current = ftell(f);

        // Verifica se a posição atual realmente deveria ser o final do
        // arquivo, de acordo com o registro de cabeçalho (duas condições):
        //
        //  - o campo `next_byte_offset` é 0 (arquivo não tem registros de
        //    dados), logo, a posição atual deveria ser a posição
        //    imediatamente após o cabeçalho
        //
        //  - a posição atual é igual à próxima posição livre
        if ((header->next_byte_offset == 0 && current != sizeof(PACKED(f_header_t)))
            || current != header->next_byte_offset)
            bail(E_PROCESSINGFILE);

        return false;
    }

    if (rec->removed != REC_REMOVED && rec->removed != REC_NOT_REMOVED)
        bail(E_PROCESSINGFILE);

    return true;
}

/**
 * Lê uma string (sem espaços) da `stdin`, abre um arquivo com
 * caminho dado por essa string, repassando o modo de abertura
 * `mode` para `fopen`. Lê o registro de cabeçalho presente no
 * arquivo e armazena-o em `*header`.
 *
 * Aborta a execução em caso de erro ou se o arquivo estiver
 * inconsistente.
 */
static FILE *file_open_from_stdin_or_bail(f_header_t *header, const char *mode)
{
    char path[PATH_MAX];
    int ret = scanf("%s", path);

    if (ret != 1)
        bail(E_PROCESSINGFILE);

    FILE *f = fopen(path, mode);

    if (!f)
        bail(E_PROCESSINGFILE);

    if (!file_read_header(f, header) || header->status != STATUS_CONSISTENT)
        bail(E_PROCESSINGFILE);

    return f;
}
 
int main(void)
{
    int func;

    // Lê a funcionalidade da `stdin`
    int ret = scanf("%d", &func);

    if (ret != 1)
        bail(E_PROCESSINGFILE);

    switch (func) {
        case FUNC_CREATE_TABLE: {
            char csv_path[PATH_MAX];
            char bin_path[PATH_MAX];

            int ret = scanf("%s %s", csv_path, bin_path);

            if (ret != 2)
                bail(E_PROCESSINGFILE);

            /* ... */

            break;
        }
        case FUNC_SELECT_STAR: {
            f_header_t header;
            
            FILE *f = file_open_from_stdin_or_bail(&header, "rb");

            bool no_matches = true;

            while (true) {
                // Devemos inicializar a struct, pois caso a leitura falhe,
                // é possível que campos de tamanho variável não inicializados
                // pela função `file_read_data_rec` sejam usados como argumento
                // da função `free` em `free_var_data_fields`.
                f_data_rec_t rec = {};

                if (!file_read_data_rec_or_bail(f, &header, &rec))
                    break;

                if (rec.removed == REC_NOT_REMOVED) {
                    file_print_data_rec(&header, &rec);
                    printf("\n");

                    no_matches = false;
                }

                free_var_data_fields(&rec);
            }

            if (no_matches)
                puts(E_NOREC);

            fclose(f);

            break;
        }
        case FUNC_SELECT_WHERE: {
            int n_queries;
            f_header_t header;

            FILE *f = file_open_from_stdin_or_bail(&header, "rb");
            
            int ret = scanf("%d", &n_queries);

            if (ret != 1)
                bail(E_PROCESSINGFILE);

            size_t n_recs = header.n_valid_recs;
            f_data_rec_t *recs = calloc(n_recs, sizeof *recs);

            // Lê todos os registros válidos e os armazena em um array, o que é mais
            // eficiente do que percorrer o arquivo várias vezes, considerando que
            // serão realizadas várias buscas consecutivamente.
            //
            // Nota-se, no entanto, que a técnica utilizada para implementar a
            // funcionalidade 2/FUNC_SELECT_STAR (guardar apenas um registro por vez)
            // é preferível caso o arquivo contenha uma quantidade extremamente alta
            // de registros, devido ao seu menor uso de memória.
            for (size_t i = 0; i < n_recs;) {
                // Espera-se que essa função nunca chegue ao final do arquivo (EOF),
                // pois estamos lendo a quantidade exata de registros que é reportada
                // no registro de cabeçalho do arquivo
                if (!file_read_data_rec_or_bail(f, &header, &recs[i]))
                    bail(E_PROCESSINGFILE);

                if (recs[i].removed == REC_NOT_REMOVED)
                    i++;
                else
                    free_var_data_fields(&recs[i]);
            }

            for (int i = 0; i < n_queries; i++) {
                int n_conds;

                // Lê a quantidade de condições de cada query
                int ret = scanf("%d", &n_conds);

                if (ret != 1)
                    bail(E_PROCESSINGFILE);

                query_t *query = query_new();

                for (int j = 0; j < n_conds; j++) {
                    size_t offset;
                    enum typeinfo info;

                    char field_repr[64];

                    // Lê o campo (representação na forma de string)
                    int ret = scanf("%s", field_repr);

                    // Recupera metadados sobre o campo que serão usados para
                    // realizar a query: offset do campo na struct `f_data_rec_t`
                    // e seu tipo (um `enum typeinfo`).
                    if (ret != 1 || !data_rec_typeinfo(field_repr, &offset, &info))
                        bail(E_PROCESSINGFILE);

                    // Irá guardar o valor referência para comparação na query
                    void *buf = NULL;
                    
                    // Usado para que possamos fazer a leitura de uma string (`T_STR`)
                    // alocada dinamicamente. Irá apontar para a string alocada dinamicamente
                    // por `parse_read_field`, nesse caso.
                    char *str;

                    // Reserva espaço para guardar o valor que será lido (e posteriormente
                    // usado na query), de acordo com o tipo do campo. Todos os valores
                    // passados para a query devem ser alocados dinamicamente. No entanto,
                    // campos do tipo `T_STR` já são alocados dinamicamente pela função
                    // `parse_read_field`, logo só precisamos guardar o ponteiro
                    // temporariamente.
                    switch (info) {
                        case T_U32:
                            buf = malloc(sizeof(uint32_t));
                            break;
                        case T_FLT:
                            buf = malloc(sizeof(float));
                            break;
                        case T_STR:
                            buf = &str;
                            break;
                    }

                    if (!parse_read_field(stdin, info, buf, NULL))
                        bail(E_PROCESSINGFILE);

                    if (info == T_STR)
                        buf = str;

                    // Adiciona uma condição de igualdade à query que consiste em:
                    // 
                    // Interpretar o valor na posição `offset` da struct `f_data_rec`
                    // com o tipo dado por `info` e verificar se seu valor é igual ao
                    // de `buf`, que possui esse mesmo tipo
                    query_add_cond_equals(query, offset, info, buf);
                }

                bool no_matches = true;

                for (size_t j = 0; j < n_recs; j++) {
                    if (!query_matches(query, &recs[j]))
                        continue;

                    file_print_data_rec(&header, &recs[j]);
                    printf("\n");

                    no_matches = false;
                }

                query_free(query);

                if (no_matches) {
                    puts(E_NOREC);
                    printf("\n");
                }

                puts("**********");
            }

            for (size_t i = 0; i < n_recs; i++)
                free_var_data_fields(&recs[i]);

            free(recs);
            fclose(f);

            break;
        }
    }
}
