///usr/bin/env clang -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer,address -I ../include ../src/index/b_tree.c $0 -o /tmp/fuzz && /tmp/fuzz "$@"; rm -f /tmp/fuzz; exit

#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "index/b_tree.h"

#define REP32(x) (x << 24 | x << 16 | x << 8 | x)
#define REP64(x) (REP32(x) << 32 | REP32(x))

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size <= 1)
        return -1;
    
    uint64_t off = 0xfa;
    uint8_t n_insertions = *data++;

    size--;

    uint32_t key;

    if (size <= n_insertions * sizeof key)
        return -1;

    remove("/tmp/b.tree");

    b_tree_index_t *tree = b_tree_open("/tmp/b.tree", "wb+");
    
    for (uint8_t i = 0; i < n_insertions; i++) {
        memcpy(&key, data, sizeof key);
        data += sizeof key;

        b_tree_insert(tree, key, REP64(off));

        off--;
    }

    b_tree_close(tree);

    return 0;
}
