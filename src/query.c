#include "query.h"

typedef struct query_cond query_cond_t;

struct query_cond {
    void *buf;
    size_t offset, len;
    bool str;

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
        free(prev);
        prev = cond;
    }

    free(prev);
    free(query);
}

void query_add_cond_equals(query_t *query, bool str, size_t offset, void *buf, size_t len)
{
    query_cond_t *cond = malloc(sizeof *cond);

    cond->str = str;
    cond->offset = offset;
    cond->buf = buf;
    cond->len = len;

    cond->next = query->conditions;
    query->conditions = cond;
}

bool query_matches(query_t *query, void *obj)
{
    if (!query->conditions)
        return true;

    for (query_cond_t *cond = query->conditions; cond; cond = cond->next) {
        if (cond->str) {
            char *buf = *(char **)(((char *)obj) + cond->offset);

            if (strncmp(buf, cond->buf, cond->len) != 0)
                return false;
        } else {
            char *buf = ((char *)obj) + cond->offset;

            if (memcmp(buf, cond->buf, cond->len) != 0)
                return false;
        }
    }

    return true;
}
