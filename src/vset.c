#include <string.h>

#include "vset.h"
#include "typeflags.h"
#include "typeinfo.h"

typedef struct vset_node vset_node_t;

struct vset_node {
    /** Guarda um valor dentre os valores do vset */
    void *val;

    /** Offset em bytes do valor em relação a um objeto base `obj` */
    size_t offset;

    /** Tipo de dado do valor armazenado em `val` */ 
    enum typeinfo info;

    uint8_t flags;

    vset_node_t *next;
};

struct vset {
    uint32_t *id;
    vset_node_t *nodes;
};

vset_t *vset_new(void)
{
    return calloc(1, sizeof(vset_t));
}

void vset_free(vset_t *vset)
{
    vset_node_t *prev = NULL;

    // Percorre a lista, desalocando cada nó percorrido.
    for (vset_node_t *curr = vset->nodes; curr; curr = curr->next) {
        if (prev) {
            free(prev->val);
            free(prev);
        }

        prev = curr;
    }

    if (prev) {
        free(prev->val);
        free(prev);
    }

    free(vset);
}

const uint32_t *vset_id(vset_t *vset)
{
    return vset->id;
}

void vset_add_value(vset_t *vset, size_t offset, enum typeinfo info, uint8_t typeflags, void *val)
{
    vset_node_t *node = malloc(sizeof *node);

    node->offset = offset;
    node->info = info;
    node->flags = typeflags;

    node->val = val;

    node->next = vset->nodes;
    vset->nodes = node;

    if (node->info == T_U32 && (node->flags & F_UNIQUE) == F_UNIQUE)
        vset->id = node->val;
}

bool vset_match_against(vset_t *vset, const void *obj, bool *unique)
{
    // O produto do conjunto vazio é 1
    if (!vset->nodes)
        return true;

    bool unique_tmp = false;

    for (vset_node_t *curr = vset->nodes; curr; curr = curr->next) {
        const char *const val = ((const char *)obj) + curr->offset;

        switch (curr->info) {
            case T_U32: {
                if (memcmp(val, curr->val, sizeof(uint32_t)) != 0)
                    return false;

                if (curr->flags & F_UNIQUE)
                    unique_tmp = true;

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

                memcpy(&a, val, sizeof a);
                memcpy(&b, curr->val, sizeof b);

                if (a != b)
                    return false;

                // Não faz sentido um float ser único

                break;
            }
            case T_STR: {
                const char *str;

                // Queremos copiar o conteúdo da região de memória apontada por `buf`
                // (sendo esse conteúdo um `char *`) para `str`, para que possamos
                // dereferenciá-lo.
                memcpy(&str, val, sizeof str);

                // Se ambas as strings forem `NULL`, são iguais, porém não podem ser únicas
                if (!str && !curr->val)
                    continue;

                if (!str || !curr->val || strcmp(str, curr->val) != 0)
                    return false;

                if (curr->flags & F_UNIQUE)
                    unique_tmp = true;

                break;
            }
            default:
                return false;
        }
    }

    if (unique)
        *unique = unique_tmp;

    return true;
}

void vset_patch(vset_t *vset, void *obj)
{
    for (vset_node_t *curr = vset->nodes; curr; curr = curr->next) {
        char *val = ((char *)obj) + curr->offset;

        switch (curr->info) {
            case T_U32:
                memcpy(val, curr->val, sizeof(uint32_t));
                break;
            case T_FLT:
                memcpy(val, curr->val, sizeof(float));
                break;
            case T_STR: {
                char *str;
                memcpy(&str, val, sizeof str);

                free(str);
                str = strdup(curr->val);

                memcpy(val, &str, sizeof str);
                
                break;
            }
        }
    }
}
