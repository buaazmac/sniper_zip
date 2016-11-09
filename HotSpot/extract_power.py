#!/usr/bin/env python
import sys, os

flr_files = ['core_layer', 'dram_layer_0', 'dram_layer_1', 'dram_layer_2', 'dram_layer_3', 'dram_layer_4']

for flr in flr_files:
	units = []
	power_trace = []

	flr_file = flr + '.flr'
	app_dir = sys.argv[1]
	power_file = app_dir + '/' + sys.argv[2]
	output_file = power_file + '.' + flr

	with open(flr_file) as f:
		for line in f:
			arr = line.split()
			if len(arr) > 1:
				units.append(arr[0])
	with open(power_file) as f:
		idx = 0
		idx_arr = []
		for line in f:
			arr = line.split()
			if idx == 0:
				for i in range(0, len(arr)):
					if arr[i] in units:
						idx_arr.append(i)	
			else:
				pp = []
				for i in idx_arr:
					pp.append(arr[i])
				power_trace.append(pp)
			idx += 1
	with open(output_file, 'w') as f:
		line = ' '.join(units) + '\n'
		f.write(line)
		for p in power_trace:
			line = ' '.join(p) + '\n'
			f.write(line)
