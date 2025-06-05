#include "defs.h"
#include "file.h"
#include "vset.h"
#include "search.h"

int64_t file_search_seq_next(FILE *f, const f_header_t *header, vset_t *vset, f_data_rec_t *rec, bool *unique)
{
    long current = ftell(f);
    
    while (current < header->next_byte_offset) {
        if (!file_read_data_rec(f, header, rec))
            return -1;

        if (rec->removed == REC_REMOVED)
            continue;

        if (vset_match_against(vset, rec, unique))
            return current;

        current = ftell(f);
    }

    return -1;
}
