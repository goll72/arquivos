#!/usr/bin/env bash
#
# Usage: diff-trees <tree-a> <tree-b>
#
# Using <tree-b> as reference, prints
# the difference from files on <tree-a>,
# but only for files that exist on <tree-a>.
# 
# This is an important distinction from running
# `diff` against the two trees directly, which
# is why this script exists in the first place.
#
# NOTE: this script assumes a flat tree (it doesn't
# check subdirectories).

set -eu

TREE_A=$1
TREE_B=$2

shopt -s globstar nullglob dotglob

{
    pushd "$TREE_A"
    FILES=(**/*)
    popd
} > /dev/null

for f in "${FILES[@]}"; do
    ./scripts/diff.sh "$TREE_A/$f" "$TREE_B/$f" || :
done
