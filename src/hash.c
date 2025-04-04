#include "hash.h"

#include <stdio.h>
#include <stdint.h>

double hash_file(FILE *f)
{
    fseek(f, 0, SEEK_SET);

    int c = 0;
    uint64_t m = 0;

    do {
        m += c;
        c = fgetc(f);
    } while (c != EOF);

    return m / 100.0;
}
