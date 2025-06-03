#include "file.h"
#include "query.h"
#include "search.h"

int64_t file_search_seq_next(FILE *f, const f_header_t *header, query_t *query, f_data_rec_t *rec, bool *unique)
{
    long current = ftell(f);
    
    while (current < header->next_byte_offset) {
        if (!file_read_data_rec(f, header, rec))
            return -1;

        if (query_matches(query, rec, unique))
            return current;

        current = ftell(f);
    }

    return -1;
}
