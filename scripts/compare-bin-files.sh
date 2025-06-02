#!/usr/bin/env bash
#
# Usage: compare-bin-files <tree-a> <tree-b>
#
# Using <tree-b> as reference, prints
# the difference from files on <tree-a>

set -eu

TREE_A=$1
TREE_B=$2

shopt -s globstar nullglob dotglob

cd "$TREE_A"
FILES=(**/*)
cd - > /dev/null

for f in "${FILES[@]}"; do
    diff -u -U100 <(hexdump -C "$TREE_A/$f") <(hexdump -C "$TREE_B/$f")
done
