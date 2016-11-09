#!/bin/bash
# $1: app, $2: power_file, $3: steady_file, $4: temp_file, $5: grid_steady_file
../hotspot -c ../hotspot.config -f core_layer.flr -p $1/$2 -steady_file $1/$3 -model_type grid -grid_layer_file test_3D.lcf;

../hotspot -c ../hotspot.config -init_file $1/$3 -f core_layer.flr -p $1/$2 -o $1/$4 -model_type grid -grid_layer_file test_3D.lcf -grid_steady_file $1/$5;
