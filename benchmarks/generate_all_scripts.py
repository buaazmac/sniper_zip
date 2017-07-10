#!/usr/bin/python

import sys, os, time, getopt, pipes, tempfile, traceback

catogories = ['cpu2006', 'parsec', 'splash2']

cpu2006_str = "perlbench bzip2 gcc bwaves gamess mcf milc zeusmp gromacs cactusADM leslie3d namd gobmk dealII soplex povray calculix hmmer sjeng GemsFDTD libquantum h264ref tonto lbm omnetpp astar wrf sphinx3 xalancbmk" 
cpu2006_arr = cpu2006_str.split()
cpu2006_size = ['train', 'ref']
parsec_str = "blackscholes bodytrack canneal dedup facesim ferret fluidanimate freqmine raytrace streamcluster swaptions vips x264"
parsec_arr = parsec_str.split()
parsec_size = ['simmedium', 'simlarge']

splash2_str = "cholesky fft fmm lu.cont radix raytrace water.nsq water.sp"
#splash2_str = "barnes cholesky fft fft_O0 fft_O1 fft_O2 fft_O3 fft_forever fft_rep2 fmm lu.cont lu.ncont ocean.cont ocean.ncont radiosity radix raytrace raytrace_opt volrend water.nsq water.sp barnes-scale fft-scale fmm-scale lu.cont-scale lu.ncont-scale ocean.cont-scale radix-scale water.nsq-scale"

#splash2_str = "cholesky fft water.sp"
splash2_arr = splash2_str.split()
splash2_size = ['small', 'large']

benchmarks = {'cpu2006': {}, 'parsec': {}, 'splash2': {}}

benchmarks['cpu2006']['app'] = cpu2006_arr
benchmarks['cpu2006']['size'] = cpu2006_size

benchmarks['parsec']['app'] = parsec_arr
benchmarks['parsec']['size'] = parsec_size

benchmarks['splash2']['app'] = splash2_arr
benchmarks['splash2']['size'] = splash2_size

def usage():
  print 'Generate Run Script for each benchmarks'
  print 'Usage: '
  print '  %s -c <benchcat> -b <benchmark> -i <inputsize> -t <tag>' % sys.argv[0]
  print 'Benchmarks: '
  print ' ', 'cpu2006: '
  print 'size: ', cpu2006_size
  print '    ', cpu2006_str
  print ' ', 'parsec: '
  print 'size: ', parsec_size
  print '    ', parsec_str
  print ' ', 'splash2: '
  print 'size: ', splash2_size
  print '    ', splash2_str
def generateScripts(path, bmname, appname, appsize, exptag):
  outputdir = exptag + '_' + bmname + '_' + appname + '_' + appsize
  with open(path, 'w') as f:
    f.write('#!/bin/bash\n#$ -N sniper\n#$ -wd /home/miz087/workplace/micro_exp/benchmarks\n#$ -j y\n#$ -o /home/miz087/workplace/results/%s.log\n#$ -l hostname=matrix0-1[0123456789]*\n#$ -S /bin/bash\n' % outputdir)
    f.write('/home/miz087/workplace/micro_exp/benchmarks/run-sniper --power -p {0}-{1} -i {2} -n 4 -c gainestown_cache -d /scratch/miz087/results/new_deadline/{3} -s stop-by-icount:5000000000\n'.format(bmname, appname, appsize, outputdir))
    f.write('mv /scratch/miz087/results/new_deadline/{0}/power_trace_log.txt /home/miz087/workplace/hpca_rst/powertrace/{1}\n'.format(outputdir, outputdir + '.power'))
    f.write('mv /scratch/miz087/results/new_deadline/{0}/test_ttrace.txt /home/miz087/workplace/hpca_rst/temptrace/{1}\n'.format(outputdir, outputdir + '.ttrace'))
    f.write('mv /scratch/miz087/results/new_deadline/{0}/sim.cfg /home/miz087/workplace/hpca_rst/config/{1}\n'.format(outputdir, outputdir + '.config'))


try:
  opts, args = getopt.getopt(sys.argv[1:], "c:i:b:t:")
except getopt.GetoptError, e:
  # print help information and exit:
  print e
  usage()
print 'hahaha'
print opts, args

cat = ''
bmname = ''
size = ''
tag = ''

for o, a in opts:
  if o == '-c':
    cat = a
  if o == '-b':
    bmname = a
  if o == '-i':
    size = a
  if o == '-t':
    tag = a
generated_scripts = []
script_home = '/home/miz087/workplace/scripts/new_deadline'
if cat == '' or cat not in catogories:
  usage()
else:
  if size not in benchmarks[cat]['size']:
    usage()
  if not os.path.exists(script_home + '/{0}'.format(tag)):
    os.system('mkdir ' + script_home +'/{0}'.format(tag))
  if bmname == '':
    print 'Generating all benchmarks scripts for', cat
    for bm in benchmarks[cat]['app']:
      script_path = script_home + '/{0}/run_{1}_{2}_{3}_{4}.sh'.format(tag, cat, bm, size, tag)
      generated_scripts.append(script_path)
      generateScripts(script_path, cat, bm, size, tag)
  else:
    print 'Generating benchmark scripts for', cat, bmname, size
    script_path = script_home + '/{0}/run_{1}_{2}_{3}_{4}.sh'.format(tag, cat, bmname, size, tag)
    generateScripts(script_path, cat, bmname, size, tag)
    generated_scripts.append(script_path)
if len(generated_scripts) != 0:
  submit_script = script_home + '/{0}/run_{1}_{2}.sh'.format(tag, cat, tag)
  with open(submit_script, 'w') as f:
    f.write('#!/bin/bash\n')
    for script in generated_scripts:
      os.system('chmod u+x ' + script)
      f.write('qsub ' + script + '\n')
  os.system('chmod u+x ' + submit_script)
    
      
  



