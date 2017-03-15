#!/bin/bash
./run-sniper --power -p parsec-$1 -i $2 -n 4 -c gainestown_cache
mv ./parsec/$1-sim$2/test_ttrace.txt ./parsec/$1-sim$2/ttrace_$3.txt
mv ./parsec/$1-sim$2/StackedDramUnison.txt ./parsec/$1-sim$2/StackedDramUnison_$3.txt

