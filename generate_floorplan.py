#!/usr/bin/env python

import os, sys, math, re, collections, buildstack, gnuplot, getopt, pprint, sniper_lib, sniper_config, sniper_stats
import math

area = {'core': {'exe': (5.71e-3, 1.43e-3),
				 'ifetch': (2.85e-3, 2.05e-3),
				 'lsu': (2.85e-3, 3.05e-3),
				 'mmu': (0.66e-3, 0.66e-3),
				 'l2': (5.71e-3, 0.79e-3),
				 'ru': (0.61e-3, 0.61e-3)},
		'L3': (11.42e-3, 4.58e-3),
		'dram': (8.00e-3, 16.00e-3)}

flr_files = ['core_layer.flr', 'dram_layer_1.flr', 'dram_layer_2.flr', 'dram_layer_3.flr', 'dram_layer_4.flr']

def init_flrfile(files):
	for fn in files:
		with file(fn, 'w') as f:
			f.write('#\n')

def draw_dram(flrfile, x, y, layer):
	with file(flrfile, 'a') as f:
		line = 'null_%d\t%f\t%f\t%f\t%f\n' \
				% (layer, float(0.011420), float(0.016000), float(0), float(0))
		f.write(line)
		
		xx, yy = x, y
		for i in xrange(32):
			unitname = 'dram_' + str(i) + '_' + str(layer)
			line = '%s\t%f\t%f\t%f\t%f\n' \
					% (unitname, float(2e-3), float(2e-3), float(xx), float(yy))
			f.write(line)
			if i % 4 == 3:
				xx = x
				yy = yy + float(2e-3)
			else:
				xx = xx + float(2e-3)

def draw_dram_ctr(flrfile, x, y):
	with file(flrfile, 'a') as f:
		xx, yy = x, y
		l = area['dram'][0]
		w = area['dram'][1]
		unitname = 'dram_ctr'
		line = '%s\t%f\t%f\t%f\t%f\n' % (unitname, float(l), float(w), float(xx), float(yy))
		f.write(line)
		'''
		for i in xrange(32):
			unitname = 'dram_ctr_' + str(i)
			line = '%s\t%f\t%f\t%f\t%f\n' \
					% (unitname, float(2e-3), float(2e-3), float(xx), float(yy))
			f.write(line)
			if i % 4 == 3:
				xx = x
				yy = yy + float(2e-3)
			else:
				xx = xx + float(2e-3)
		'''

def draw_l3(flrfile, x, y):
	with file(flrfile, 'a') as f:
		line = 'L3\t%f\t%f\t%f\t%f\n' \
			   % (float(area['L3'][0]), float(area['L3'][1]), \
					float(x), float(y))
		f.write(line)

def draw_core(flrfile, x, y, coreid):
	with file(flrfile, 'a') as f:
		cu = area['core']

		xx = 0
		yy = 5.71e-3 - float(cu['exe'][1])
		line = 'exe_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['exe'][0]), float(cu['exe'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)
		
		xx = 0
		yy = 5.71e-3 - float(cu['exe'][1]) - float(cu['lsu'][1])
		line = 'ifetch_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['ifetch'][0]), float(cu['ifetch'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		xx = float(cu['ifetch'][0])
		yy = 5.71e-3 - float(cu['exe'][1]) - float(cu['lsu'][1])
		line = 'lsu_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['lsu'][0]), float(cu['lsu'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		xx = 0
		yy = 0
		line = 'mmu_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['mmu'][0]), float(cu['mmu'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		xx = 0
		yy = 5.71e-3 - float(cu['exe'][1]) - float(cu['lsu'][1]) - float(cu['l2'][1])
		line = 'l2_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['l2'][0]), float(cu['l2'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		xx = 0
		yy = 5.71e-3 - float(cu['exe'][1]) - float(cu['ru'][1])
		line = 'ru_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['ru'][0]), float(cu['ru'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

def main():
# Init floor plan files
	init_flrfile(flr_files)

# Draw L3 Cache in core layer
	draw_l3(flr_files[0], 0, 0)
# Draw CPU - 4 cores
	draw_core(flr_files[0], 0, 4.58e-3, 0)
	draw_core(flr_files[0], 5.71e-3, 4.58e-3, 1)
	draw_core(flr_files[0], 0, 10.29e-3, 2)
	draw_core(flr_files[0], 5.71e-3, 10.29e-3, 3)
# Draw DRAM : core_layer - controller, layer 1 - 4
	draw_dram_ctr(flr_files[0], 11.42e-3, 0)
	draw_dram(flr_files[1], 11.42e-3, 0, 0)
	draw_dram(flr_files[2], 11.42e-3, 0, 1)
	draw_dram(flr_files[3], 11.42e-3, 0, 2)
	draw_dram(flr_files[4], 11.42e-3, 0, 3)

if __name__ == '__main__':
	main()
	
