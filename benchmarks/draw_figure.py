#!/usr/bin/env python

import sys, os
import numpy as np
import matplotlib.pyplot as plt
import re

home = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/benchmarks/')))
sniperdir = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/')))
tooldir = os.path.join(sniperdir, 'tools')

if sys.argv[1] == 'splash2':
	os.system('pwd')
	work_dir = os.path.join(home, 'splash2/splash2/' + sys.argv[2])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')
elif sys.argv[1] == 'parsec':
	os.system('pwd')
	work_dir = os.path.join(home, 'parsec/' + sys.argv[2])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')
else:
	print "unrecognized benchmark type"

temp_trace = []
access_trace = []
missrate_trace = []

flag = 0

with open(sys.argv[3]) as f:
	dram_cntlr_temp = []
	dram_cntlr_access = []
	cache_miss_rate = 0.0
	for line in f:
		if line[0] == "-":
			if flag == 0:
				flag = 1
				continue
			tot_access = reduce(lambda x,y: x+y, dram_cntlr_access)
			if tot_access > 50000:
				temp_trace.append(dram_cntlr_temp)
				access_trace.append(dram_cntlr_access)
				missrate_trace.append(cache_miss_rate)
			dram_cntlr_temp = []
			dram_cntlr_access = []
		else:
			arr = line.split()
			if arr[2][:9] == 'dram_ctlr':
				dram_cntlr_temp.append(float(arr[4][:-1]))
				dram_cntlr_access.append(int(arr[10]))
			elif arr[0][0] == 'T':
				if float(arr[3])!=0:
					cache_miss_rate = float(arr[3])

n_bins = len(temp_trace[0])
bar_width = 0.4
opacity = 0.4
for i in range(len(temp_trace)):
	x = np.arange(n_bins)
	ya = access_trace[i]
	yb = temp_trace[i]

	fig, ax1 = plt.subplots()
	ax2 = ax1.twinx()
	ax1.bar(x, ya, bar_width, alpha=opacity, color='b', label='Access')
	ax1.set_ylabel('Access')
	ax2.bar(x + bar_width, yb, bar_width, alpha=opacity, color='r', label='Temperature')

	max_temp = 0
	index = 0
	for ii, value in enumerate(yb):
		if value > max_temp:
			max_temp = value
			index = ii
	ax2.text(index, max_temp, str(max_temp))
	ax2.text(1, 130, 'MissRate; ' + str(missrate_trace[i]))

	ax2.set_ylabel('Temperature')
	ax2.set_ylim(0, 150)
	file_name = sys.argv[3] + '_' + str(i) + '.png'
	plt.savefig(file_name)



os.chdir(home)
os.system('pwd')
