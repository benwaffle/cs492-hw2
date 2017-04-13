#!/bin/bash

ulimit -c 0
for alg in FIFO LRU CLOCK; do
    for pp in + -; do
        for ps in 1 2 4 8 16 32; do
            echo "===================";
            echo ./main plist.txt ptrace.txt $ps $alg $pp;
            ./main plist.txt ptrace.txt $ps $alg $pp | tail -n1;
        done
    done
done
