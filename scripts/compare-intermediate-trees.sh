#!/usr/bin/env bash

if [ $# -ne 5 ]; then
    echo "Usage: $0 <prog1> <prog2> <input-files-dir> <out-file> <test-in>" >&2
    exit 1 
fi

PROG1=$(realpath $1)
PROG2=$(realpath $2)
INDIR=$3

OUT=$4

TEST=$5

set -x

P1DIR=$(mktemp -d)
P2DIR=$(mktemp -d)

set +x

mkfifo $P1DIR/in.fifo
mkfifo $P2DIR/in.fifo

trap "rm -rf $P1DIR $P2DIR" EXIT INT HUP

mkdir -p $P1DIR/work $P2DIR/work

cp -R $INDIR/* $P1DIR/work
cp -R $INDIR/* $P2DIR/work

(cd $P1DIR/work && $PROG1 < ../in.fifo > ../out 2>&1) &
P1PID=$!

(cd $P2DIR/work && $PROG2 < ../in.fifo > ../out 2>&1) &
P2PID=$!

TTY=$(tty)

trap "kill $P1PID $P2PID 2>/dev/null" EXIT INT HUP

{
    echo "Running $1 and $2 on $TEST and comparing their outputs for $OUT..."
    read line

    echo "$line" >&5
    echo "$line" >&6

    i=0

    while read line; do
        echo "Iteration $i"
        
        echo "$line" >&5
        echo "$line" >&6

        sleep 1

        ./arquivos/scripts/dump_b_tree $P1DIR/work/$OUT > $P1DIR/dump
        ./arquivos/scripts/dump_b_tree $P2DIR/work/$OUT > $P2DIR/dump
    
        git diff --no-index --no-prefix -U1000 $P1DIR/dump $P2DIR/dump </dev/tty 1>/dev/tty 2>/dev/tty

        i=$((i + 1))

        echo "Type something to continue..."
        read </dev/tty
    done
} < $TEST 5<>$P1DIR/in.fifo 6<>$P2DIR/in.fifo
