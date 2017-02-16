#!/usr/bin/env python

import sys, os

home = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/benchmarks/')))
sniperdir = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/')))
tooldir = os.path.join(sniperdir, 'tools')

runcmd = tooldir + '/mcpat_fine_grain.py'
power_trace_cmd = tooldir + '/generate_powertrace.py'

if sys.argv[2] == 'splash':
	os.system('pwd')
	work_dir = os.path.join(home, 'splash2/splash2/' + sys.argv[3])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')
else:
	os.system('pwd')
	work_dir = os.path.join(home, 'parsec/' + sys.argv[3])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')

if sys.argv[1] == 'cache':
	print 'hehe'
	os.system(runcmd + ' -m cache')
	os.system(power_trace_cmd + ' core.stats  cache.stats ' + sys.argv[4])
elif sys.argv[1] == 'mem':
	runcmd = runcmd + ' -m mem'
	os.system(runcmd)
	os.system(power_trace_cmd + ' core.stats  mem.stats ' + sys.argv[4])
else:
	print '[run mcpat fine grain]error: unrecognized model'

os.chdir(home)
os.system('pwd')
