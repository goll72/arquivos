#include <err.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <stdnoreturn.h>

#include "defs.h"
#include "file.h"
#include "query.h"
#include "error.h"

#include "util/hash.h"

enum functionality {
    FUNC_CREATE_TABLE = 1,
    FUNC_SELECT_STAR = 2,
    FUNC_SELECT_WHERE = 3,
};

/**
 * Apaga os campos de tamanho variável do registro `reg`.
 */
static void free_var_data_fields(f_data_reg_t *reg)
{
#define X(...)
#define Y(T, name, ...) free(reg->name);

    DATA_REG_FIELDS(X, X, Y)

#undef X
#undef Y
}

/**
 * Aborta a execução do programa, imprimindo a mensagem `msg`.
 *
 * NOTE: a função `errx` não é usada, pois imprime a mensagem
 * na stream `stderr`.
 */
static void bail(char *msg)
{
    puts(msg);
    exit(1);
}

/**
 * Idêntica à função `file_read_data_reg`, porém verifica se
 * o resultado da tentativa de leitura é coerente. Se não for,
 * aborta a execução do programa.
 *
 * Retorna `true` se ainda não tiver chegado ao final do arquivo.
 */
static bool file_read_data_reg_or_bail(
    FILE *f, const f_header_t *header, f_data_reg_t *reg)
{

    if (!file_read_data_reg(f, header, reg)) {
        free_var_data_fields(reg);

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

    return true;
}

int main(void)
{
    enum functionality func;
    int ret = scanf("%d", &func);

    if (ret != 1)
        bail(E_PROCESSINGFILE);

    switch (func) {
        case FUNC_CREATE_TABLE: {
            char csv_file[PATH_MAX];
            char bin_file[PATH_MAX];

            int ret = scanf("%s %s", csv_file, bin_file);

            if (ret != 2)
                bail(E_PROCESSINGFILE);

            /* ... */
        }

        break;
        case FUNC_SELECT_STAR: {
            char bin_file[PATH_MAX];

            int ret = scanf("%s", bin_file);

            if (ret != 1)
                bail(E_PROCESSINGFILE);

            FILE *f = fopen(bin_file, "rb");

            if (!f)
                bail(E_PROCESSINGFILE);

            f_header_t header;

            if (!file_read_header(f, &header))
                bail(E_PROCESSINGFILE);

            while (true) {
                f_data_reg_t reg = {};

                if (!file_read_data_reg_or_bail(f, &header, &reg))
                    break;

                if (reg.removed == '0') {
                    file_print_data_reg(&header, &reg);
                    printf("\n");
                }

                free_var_data_fields(&reg);
            }

            // Imprime o hash do arquivo, equivalente à função binarioNaTela
            printf("%lf\n", hash_file(f));
        }

        break;
        case FUNC_SELECT_WHERE: {
            int n_queries;
            char bin_file[PATH_MAX];

            int ret = scanf("%s %d", bin_file, &n_queries);

            if (ret != 2)
                bail(E_PROCESSINGFILE);

            FILE *f = fopen(bin_file, "rb");

            if (!f)
                bail(E_PROCESSINGFILE);

            f_header_t header;

            if (!file_read_header(f, &header))
                bail(E_PROCESSINGFILE);

            f_data_reg_t *regs = calloc(header.n_valid_regs, sizeof *regs);

            // Registros removidos também são buscados
            for (int i = 0; i < header.n_valid_regs; i++) {
                if (!file_read_data_reg_or_bail(f, &header, &regs[i])) {
                    bail(E_PROCESSINGFILE);
                }
            }

            for (int i = 0; i < n_queries; i++) {
                int m;
                bool no_matches = false;

                ret = scanf("%d", &m);

                if (ret != 1)
                    bail(E_PROCESSINGFILE);

                query_t *query = query_new();

                bool is_str;
                size_t offset, len;

                if (!data_reg_typeinfo("" /* XXX: ... */, &offset, &len, &is_str))
                    bail(E_PROCESSINGFILE);

                query_add_cond_equals(query, is_str, offset, NULL, len);

                /* XXX: ... */

                for (int j = 0; j < header.n_valid_regs; j++) {
                    if (query_matches(query, &regs[j])) {
                        if (regs[j].removed == '0') {
                            file_print_data_reg(&header, &regs[j]);
                            printf("\n");
                        } else {
                            puts(E_NOREG);
                        }

                        no_matches = false;
                    }
                }

                if (no_matches)
                    puts(E_NOREG);

                query_free(query);

                puts("**********");
            }

            for (int i = 0; i < header.n_valid_regs; i++)
                free_var_data_fields(&regs[i]);

            free(regs);
        }

        break;
    }
}
