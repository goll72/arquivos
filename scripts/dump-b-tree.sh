#!/bin/sh

set -e

FILE="$1"

split -a 3 -b 44 -d -- "$FILE" "$FILE".

printf '      '

hexdump -L=always -e '44/1 " %02x_L[blue@0,green@1-4,yellow@5-8,cyan@9-12,gray@13-43]" " "' -e '44/1 "%_p" "\n"' "$FILE".000
rm "$FILE".000

echo
echo "       type        n_keys      child       key         offset                  child       key         offset                  child"

i=0

for f in "$FILE".*; do
    hexdump -L=always -e '44/1 " %02x_L[gray@0-3,green@4-7,blue@8-11,yellow@12-15,red@16-23,blue@24-27,yellow@28-31,red@32-39,blue@40-43]" " "' -e '44/1 "%_p" "\n"' "$f"
done | while read -r line; do
    printf '%05d %s\n' $i "$line"
    i=$((i + 1))
done

rm "$FILE".*
