#ifndef MEM_H
#define MEM_H

#include <string.h>
#include <stdlib.h>

/**
 * Análogo à função `mempcpy`, porém para a função `memmove`.
 */
static inline void *mempmove(void *dest, void *src, size_t n)
{
    return (char *)memmove(dest, src, n) + n;
}

#endif /* MEM_H */
