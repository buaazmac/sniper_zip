#!/usr/bin/env python

import sys, os

home = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/benchmarks/')))
sniperdir = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/')))
tooldir = os.path.join(sniperdir, 'tools')

runcmd = tooldir + '/mcpat_fine_grain.py'

if sys.argv[2] == 'splash':
	os.system('pwd')
	work_dir = os.path.join(home, 'splash2/splash2/' + sys.argv[3])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')

if sys.argv[1] == 'cache':
	print 'hehe'
	os.system(runcmd + ' -m cache')
elif sys.argv[1] == 'mem':
	runcmd = runcmd + ' -m mem'
	os.system(runcmd)
else:
	print '[run mcpat fine grain]error: unrecognized model'

os.chdir(home)
os.system('pwd')
