#!/usr/bin/env python
import sys, os

flr_files = ['core_layer', 'dram_layer_0', 'dram_layer_1', 'dram_layer_2', 'dram_layer_3', 'dram_layer_4']

for flr in flr_files:
	rst = {}
	dram_stats = {}

	flr_file = flr + '.flr'
	app_dir = sys.argv[1]
	tmp_file = app_dir + '/' + sys.argv[2]
	dram_stats_file = app_dir + '/' + sys.argv[3]
	output_file = tmp_file + '.' + flr
	
	with open(dram_stats_file) as f:
		for line in f:
			arr = line.split()
			if len(arr) > 2:
				dram_stats[arr[0]] = line[len(arr[0])+1:]
	
	with open(flr_file) as f:
		for line in f:
			arr = line.split()
			if len(arr) > 1:
				rst[arr[0]] = 0	
	unit_arr = []
	tmp_arr = []
	with open(tmp_file) as f:
		idx = 0
		for line in f:
			if idx == 0:
				unit_arr = line.split()
				idx += 1
			else:
				tmp_arr = line.split()
		idx = 0
		for unit in unit_arr:
			if unit in rst.keys():
				rst[unit] = tmp_arr[idx]
			idx += 1
	with open(output_file, 'w') as f:
		rst_keys = sorted(rst.keys())
		for unit in rst_keys:
			dram_stats_str = '\n'
			if unit in dram_stats.keys():
				dram_stats_str = dram_stats[unit]
			f.write('{0}\t{1}\t{2}'.format(unit, rst[unit], dram_stats_str))
