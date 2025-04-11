#include <string.h>

#include "query.h"

typedef struct query_cond query_cond_t;

struct query_cond {
    void *buf;
    size_t offset;
    enum typeinfo info;

    query_cond_t *next;
};

struct query {
    query_cond_t *conditions;
};

query_t *query_new(void)
{
    return calloc(1, sizeof(query_t));
}

void query_free(query_t *query)
{
    query_cond_t *prev = NULL;

    for (query_cond_t *cond = query->conditions; cond; cond = cond->next) {
        free(prev->buf);
        free(prev);

        prev = cond;
    }

    free(prev->buf);
    free(prev);

    free(query);
}

void query_add_cond_equals(query_t *query, size_t offset, enum typeinfo info, void *buf)
{
    query_cond_t *cond = malloc(sizeof *cond);

    cond->offset = offset;
    cond->info = info;

    cond->buf = buf;

    cond->next = query->conditions;
    query->conditions = cond;
}

bool query_matches(query_t *query, const void *obj)
{
    // O produto do conjunto vazio é 1
    if (!query->conditions)
        return true;

    for (query_cond_t *cond = query->conditions; cond; cond = cond->next) {
        const char *const buf = ((const char *)obj) + cond->offset;

        switch (cond->info) {
            case T_U32: {
                if (memcmp(buf, cond->buf, sizeof(uint32_t)) != 0)
                    return false;

                break;
            }
            case T_FLT: {
                // Não podemos apenas fazer uma comparação binária byte-a-byte
                // (ex. memcmp), pois certos valores do tipo `float` são iguais,
                // embora admitam uma representação binária diferente e vice-versa:
                //
                //  `0.0f == -0.0f` mas possuem representação binária diferente;
                //
                //  `nanf(a) != nanf(b)` independentemente dos valores de `a` e `b`.
                float a, b;

                memcpy(&a, buf, sizeof a);
                memcpy(&b, cond->buf, sizeof b);

                if (a != b)
                    return false;

                break;
            }
            case T_STR: {
                const char *str;

                // Queremos copiar o conteúdo da região de memória apontada por `buf`
                // (sendo esse conteúdo um `char *`) para `str`, para que possamos
                // dereferenciá-lo.
                memcpy(&str, buf, sizeof str);

                if (!str && !cond->buf)
                    continue;

                if (!str || !cond->buf || strcmp(str, cond->buf) != 0)
                    return false;

                break;
            }
            default:
                return false;
        }
    }

    return true;
}
