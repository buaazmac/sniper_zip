#!/usr/bin/env python

import sys, os

chosen_data = ['all', 'dram_cntlr_0', 'dram_cntlr_4', 'dram_cntlr_8']
metrics = ['avg', 'max']
datas = {}
for data in chosen_data:
	datas[data] = {}
	for metric in metrics:
		datas[data][metric] = 0;

max_temp_list = []
avg_temp_list = []
unit_list = []

max_temp = 0
hottest_unit = ''

tot_temp = 0.0
num = 0.0

with open(sys.argv[1]) as f:
	for line in f:
		if line[0] == '-':
			if max_temp != 0:
				max_temp_list.append(max_temp)
				unit_list.append(hottest_unit)
				avg_temp_list.append(tot_temp / num)
			max_temp = 0
			hottest_unit = ''
			tot_temp = 0.0
			num = 0.0
		else:
			arr = line.split()
			tot_temp += float(arr[4])
			num += 1.0
			if max_temp < float(arr[4]):
				max_temp = float(arr[4])
				hottest_unit = arr[2][:-1]

print max_temp_list
print unit_list
print avg_temp_list

