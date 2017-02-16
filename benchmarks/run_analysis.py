#!/usr/bin/env python

import sys, os

home = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/benchmarks/')))
sniperdir = os.path.abspath(os.path.join(os.path.dirname('/media/zmac/extended/3ddram/sniper/')))
tooldir = os.path.join(sniperdir, 'tools')

runcmd = tooldir + '/thermal_analysis.py'

if sys.argv[1] == 'splash':
	os.system('pwd')
	work_dir = os.path.join(home, 'splash2/splash2/' + sys.argv[2])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')
else:
	os.system('pwd')
	work_dir = os.path.join(home, 'parsec/' + sys.argv[2])
	print work_dir
	os.chdir(work_dir)
	os.system('pwd')

os.system(runcmd + ' ' + sys.argv[3])

os.chdir(home)
os.system('pwd')
