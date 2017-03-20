#!/bin/bash
echo '---------------------BeginTime-----------------'
date
echo '---------------------***-------------------'

./run-sniper --power -p cpu2006-$1 -i $2 -n 4 -c gainestown_cache -d /root/micro/workplace/results/$1_$2_$3

echo '---------------------EndTime-----------------'
date
echo '---------------------***-------------------'
#mv ./parsec/$1-sim$2/test_ttrace.txt ./parsec/$1-sim$2/ttrace_$3.txt
#mv ./parsec/$1-sim$2/StackedDramUnison.txt ./parsec/$1-sim$2/StackedDramUnison_$3.txt
