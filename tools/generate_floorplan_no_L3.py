#!/usr/bin/env python

import os, sys, math, re, collections, buildstack, gnuplot, getopt, pprint, sniper_lib, sniper_config, sniper_stats
import math

area = {'core': {'ialu': (1.00e-3, 1.50e-3),
				 'fpalu': (1.00e-3, 1.50e-3),
				 'inssch': (2.00e-3, 1.00e-3),
				 'l1i': (2.00e-3, 2.00e-3),
				 'insdec': (1.00e-3, 2.00e-3),
				 'bp': (1.00e-3, 1.00e-3),
				 'ru': (1.00e-3, 1.00e-3),
				 'l1d': (2.00e-3, 2.00e-3),
				 'mmu': (2.00e-3, 0.50e-3),
				 'l2': (4.00e-3, 1.50e-3)},
		'dram': (8.00e-3,12.00e-3)}
core_area = (4.00e-3, 6.00e-3)

flr_files = ['core_layer.flr', 'dram_layer_0.flr', 'dram_layer_1.flr', 'dram_layer_2.flr', 'dram_layer_3.flr', 'dram_layer_4.flr']

def init_flrfile(files):
	for fn in files:
		with file(fn, 'w') as f:
			f.write('#\n')

def draw_dram(flrfile, x, y, layer):
	with file(flrfile, 'a') as f:
		
		xx, yy = x, y
		width = float(area['dram'][0]) / float(4)
		length = float(area['dram'][1]) / float(8)
		for i in xrange(32):
			unitname = 'dram_' + str(i) + '_' + str(layer * 2)
			line = '%s\t%f\t%f\t%f\t%f\n' \
					% (unitname, width / 2.0, length, float(xx), float(yy))
			f.write(line)
			unitname = 'dram_' + str(i) + '_' + str(layer * 2 + 1)
			line = '%s\t%f\t%f\t%f\t%f\n' \
					% (unitname, width / 2.0, length, float(xx + width / 2.0), float(yy))
			f.write(line)
			if i % 4 == 3:
				xx = x
				yy = yy + length
			else:
				xx = xx + width

def draw_dram_ctlr(flrfile, x, y):
	with file(flrfile, 'a') as f:
		xx, yy = x, y
		unitname = 'dram_ctlr'
		width = float(area['dram'][0]) / 4.0
		length = float(area['dram'][1]) / 8.0
		for i in xrange(32):
			unitname = 'dram_ctlr_' + str(i)
			line = '%s\t%f\t%f\t%f\t%f\n' \
					% (unitname, width, length, float(xx), float(yy))
			f.write(line)
			if i % 4 == 3:
				xx = x
				yy = yy + length
			else:
				xx = xx + width

def draw_core(flrfile, x, y, coreid):
	with file(flrfile, 'a') as f:
		cu = area['core']

		# integer alus
		xx = float(cu['l1d'][0])
		yy = float(cu['l2'][1])
		line = 'ialu_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['ialu'][0]), float(cu['ialu'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)
		
		# float point alus
		xx = float(cu['l1d'][0]) + float(cu['ialu'][0])
		yy = float(cu['l2'][1])
		line = 'fpalu_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['fpalu'][0]), float(cu['fpalu'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# instrcution scheduler
		xx = float(cu['l1d'][0])
		yy = float(cu['l2'][1]) + float(cu['ialu'][1])
		line = 'inssch_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['inssch'][0]), float(cu['inssch'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# l1-i cache
		xx = 0
		yy = float(cu['l2'][1]) + float(cu['l1d'][1]) + float(cu['mmu'][1])
		line = 'l1i_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['l1i'][0]), float(cu['l1i'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# instruction decode
		xx = float(cu['l1i'][0])
		yy = float(cu['l2'][1]) + float(cu['l1d'][1]) + float(cu['mmu'][1])
		line = 'insdec_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['insdec'][0]), float(cu['insdec'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# branch prediction
		xx = float(cu['l1i'][0]) + float(cu['insdec'][0])
		yy = float(cu['l2'][1]) + float(cu['l1d'][1]) + float(cu['mmu'][1]) + float(cu['ru'][1])
		line = 'bp_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['bp'][0]), float(cu['bp'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# renaming unit
		xx = float(cu['l1i'][0]) + float(cu['insdec'][0])
		yy = float(cu['l2'][1]) + float(cu['l1d'][1]) + float(cu['mmu'][1])
		line = 'ru_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['ru'][0]), float(cu['ru'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# l1-d cache
		xx = 0
		yy = float(cu['l2'][1])
		line = 'l1d_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['l1d'][0]), float(cu['l1d'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# memory management unit and bus
		xx = 0
		yy = float(cu['l2'][1]) + float(cu['l1d'][1])
		line = 'mmu_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['mmu'][0]), float(cu['mmu'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

		# l2 cache
		xx = 0
		yy = 0 
		line = 'l2_%d\t%f\t%f\t%f\t%f\n' % \
			   (coreid, float(cu['l2'][0]), float(cu['l2'][1]),\
				float(x + xx), float(y + yy))
		f.write(line)

def main():
# Init floor plan files
	init_flrfile(flr_files)

# Draw L3 Cache in core layer
#draw_l3(flr_files[0], 0, 0)
# Draw CPU - 4 cores
	draw_core(flr_files[0], 0, 0, 0)
	draw_core(flr_files[0], 0, float(core_area[1]), 1)
	draw_core(flr_files[0], float(core_area[0]), 0, 2)
	draw_core(flr_files[0], float(core_area[0]), float(core_area[1]) , 3)
# Draw DRAM : core_layer - controller, layer 1 - 4
	draw_dram_ctlr(flr_files[1], 0, 0)
	draw_dram(flr_files[2], 0, 0, 0)
	draw_dram(flr_files[3], 0, 0, 1)
	draw_dram(flr_files[4], 0, 0, 2)
	draw_dram(flr_files[5], 0, 0, 3)

if __name__ == '__main__':
	main()
	
