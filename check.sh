#!/bin/bash

function go {
    ./main plist.txt ptrace.txt $1 $2 $3 | sed 's/.*: //'
}

ulimit -c 0
for alg in FIFO LRU CLOCK; do
    for pp in + -; do
        for ps in 1 2 4 8 16 32; do
            #echo "===================";
            #echo ./main plist.txt ptrace.txt $ps $alg $pp;
            echo "$alg,$ps,$pp" $(go $ps $alg $pp);
        done
    done
done
