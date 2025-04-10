#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "defs.h"
#include "hash.h"
#include "file.h"
#include "error.h"

enum functionality {
    FUNC_CREATE_TABLE = 1,
    FUNC_SELECT_STAR = 2,
    FUNC_SELECT_WHERE = 3,
};

static void free_var_data_fields(f_data_reg_t *reg)
{
#define X(_, name)
#define Y(_, name) free(reg->name);

    DATA_REG_FIELDS(X, X, Y)

#undef X
#undef Y
}

int main(void)
{
    enum functionality func;
    int ret = scanf("%d", &func);

    if (ret != 1) {
        puts(E_PROCESSINGFILE);
        return 1;
    }

    switch (func) {
        case FUNC_CREATE_TABLE: {
            char csv_file[PATH_MAX];
            char bin_file[PATH_MAX];

            int ret = scanf("%s %s", csv_file, bin_file);

            if (ret != 2) {
                puts(E_PROCESSINGFILE);
                return 1;
            }

            /* ... */
        }

        break;
        case FUNC_SELECT_STAR: {
            char bin_file[PATH_MAX];

            int ret = scanf("%s", bin_file);

            if (ret != 1) {
                puts(E_PROCESSINGFILE);
                return 1;
            }

            FILE *f = fopen(bin_file, "rb");

            if (!f) {
                puts(E_PROCESSINGFILE);
                return 1;
            }

            f_header_t header;
            file_read_header(f, &header);

            while (true) {
                f_data_reg_t reg = {};

                if (!file_read_data_reg(f, &header, &reg)) {
                    free_var_data_fields(&reg);

                    long current = ftell(f);

                    // Verifica se a posição atual realmente deveria ser o final do
                    // arquivo, de acordo com o registro de cabeçalho (duas condições):
                    //
                    //  - o campo `next_byte_offset` é 0 (arquivo não tem registros de
                    //    dados), logo, a posição atual deveria ser a posição
                    //    imediatamente após o cabeçalho
                    //
                    //  - a posição atual é igual à próxima posição livre
                    if ((header.next_byte_offset == 0
                            && current != sizeof(PACKED(f_header_t)))
                        || current != header.next_byte_offset) {
                        puts(E_PROCESSINGFILE);
                        return 1;
                    }

                    break;
                }

                file_print_data_reg(&header, &reg);

                free_var_data_fields(&reg);
            }

            // Imprime o hash do arquivo, equivalente à função binarioNaTela
            printf("%lf\n", hash_file(f));
        }

        break;
        case FUNC_SELECT_WHERE:
            /* ... */
            break;
    }
}
