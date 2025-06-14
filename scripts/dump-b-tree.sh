#!/bin/sh

set -e

while :; do
    case "$1" in
        -C)
            CHAR_FMT=1
            ;;
        -p)
            PAGE_SIZE=$2
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
    esac

    shift
done

FILE="$1"
PAGE_SIZE=${PAGE_SIZE:-44}

split -a 3 -b "$PAGE_SIZE" -d -- "$FILE" "$FILE".

printf '      '

len=0

if [ -n "$CHAR_FMT" ]; then
    SEP=" "
else
    SEP="\n"
fi

hexdump -L=always -e "$PAGE_SIZE/1 \" %02x_L[blue@0,green@1-4,yellow@5-8,cyan@9-12,gray@13-$((PAGE_SIZE - 1))]\" \"$SEP\"" ${CHAR_FMT:+-e} ${CHAR_FMT:+"$PAGE_SIZE/1 \"%_p\" \"\n\""} "$FILE".000
rm "$FILE".000

printf '\n'
printf '       type        n_keys      child'

# type + n_keys + child
len=$((len + 12))

hex_colors="gray@0-3,green@4-7,blue@8-11"

while [ $((len + 16)) -le $PAGE_SIZE ]; do
    printf '       key         offset                  child'

    hex_colors="$hex_colors,yellow@$len-$((len + 3)),red@$((len + 4))-$((len + 11)),blue@$((len + 12))-$((len + 15))"
    len=$((len + 16))
done

printf '\n'

i=0

for f in "$FILE".*; do
    hexdump -L=always -e "$PAGE_SIZE/1 \" %02x_L[$hex_colors]\" \"$SEP\"" ${CHAR_FMT:+-e} ${CHAR_FMT:+"$PAGE_SIZE/1 \"%_p\" \"\n\""} "$f"
done | while read -r line; do
    printf '%05d %s\n' $i "$line"
    i=$((i + 1))
done

rm "$FILE".*
