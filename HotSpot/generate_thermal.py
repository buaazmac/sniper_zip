#!/usr/bin/env python
import sys, os

# argv[1]: app, argv[2]: cache/mem

hotspot_home = os.path.abspath(os.path.join(os.path.dirname('..')))
home = os.path.abspath(os.path.join(os.path.dirname('.')))
work_path = os.path.join(home, sys.argv[1])
print work_path
runcmd = home + '/run_thermal.sh'
drawcmd = '../grid_thermal_map.pl'

input_file = 'cache.stats.trace'
interval_power_trace = 'powertrace.input'
if sys.argv[2] == 'mem':
	input_file = 'mem.stats.trace'
datas = []
with open(os.path.join(work_path, input_file)) as f:
	for line in f:
		datas.append(line)
idx = 0
for data in datas[1:]:
	with open(work_path + '/' + interval_power_trace, 'w') as f:
		f.write(datas[0])
		f.write(data + data)
	ttrace_file = input_file + '.ttrace' + str(idx)
	tmp_runcmd = runcmd + ' ' + sys.argv[1] + ' ' + interval_power_trace + ' tmp.steady ' + ttrace_file + ' tmp.grid.steady'
	print 'run: ' , tmp_runcmd
	os.system(tmp_runcmd)
	map_input_file = work_path + '/tmp.grid.steady'
	map_output = work_path + '/' + input_file + str(idx) + '.svg'
	tmp_drawcmd = drawcmd + ' core_layer.flr ' + map_input_file + ' > ' + map_output
	print 'draw: ', tmp_drawcmd
	os.system(tmp_drawcmd)
	idx += 1
