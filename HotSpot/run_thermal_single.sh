#!/bin/bash
# $1: app, $2: power_file, $3: steady_file, $4: temp_file, $5: layer
../hotspot -c ../hotspot.config -f $5\.flr -p $1/$2\.$5 -steady_file $1/$3\.$5

../hotspot -c ../hotspot.config -init_file $1/$3\.$5 -f $5\.flr -p $1/$2\.$5 -o $1/$4\.$5
