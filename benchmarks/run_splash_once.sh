#!/bin/bash
./run-sniper --power -p splash2-$1 -i $2 -n 4 -c gainestown_cache
mv ./splash2/splash2/$1-$2/test_ttrace.txt ./splash2/splash2/$1-$2/ttrace_$3.txt
mv ./splash2/splash2/$1-$2/BasicStatistics.txt ./splash2/splash2/$1-$2/BasicStatistics_$3.txt
mv ./splash2/splash2/$1-$2/runtime_data.txt ./splash2/splash2/$1-$2/runtime_data_$3.txt
#./run-sniper --power -p splash2-barnes -i small -n 4 -c gainestown_cache
#./run-sniper --power -p splash2-cholesky -i small -n 4 -c gainestown_cache
#./run-sniper --power -p splash2-radix -i small -n 4 -c gainestown_cache

