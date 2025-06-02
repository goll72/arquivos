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
    pretty=$(sed 's;/;-;g' <<< "$f")

    hexdump -C "$TREE_A/$f" > /tmp/output.$pretty
    hexdump -C "$TREE_B/$f" > /tmp/expected.$pretty

    git diff --no-index -U100 /tmp/output.$pretty /tmp/expected.$pretty

    rm /tmp/output.$pretty /tmp/expected.$pretty 
done
