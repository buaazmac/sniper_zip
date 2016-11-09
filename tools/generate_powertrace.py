#!/usr/bin/env python

import os, sys, math, re, collections, buildstack, gnuplot, getopt, pprint, sniper_lib, sniper_config, sniper_stats
import math

import PowerTable as pt
import DramPowerConfig as dram_config

def compute_dram_power(act_t, pre_t, read_t, write_t, t, page_hit_rate):
	maxVcc = float(dram_config.config['maxVcc'][0])
	Vdd = float(dram_config.config['Vdd'][0])
	Idd2P = float(dram_config.config['Idd2P'][0])
	Idd2N = float(dram_config.config['Idd2N'][0])
	Idd3P = float(dram_config.config['Idd3P'][0])
	Idd3N = float(dram_config.config['Idd3N'][0])
	Idd5 = float(dram_config.config['Idd5'][0])
	Idd4W = float(dram_config.config['Idd4W'][0])
	Idd0 = float(dram_config.config['Idd0'][0])
	RFC_min = float(dram_config.config['RFC_min'][0])
	REFI = float(dram_config.config['REFI'][0])
	tRAS = float(dram_config.config['tRAS'][0])
	tRC = float(dram_config.config['tRC'][0])
	tCKavg = float(dram_config.config['tCKavg'][0])
#tRRDsch = float(dram_config.config['tRRDsch'][0])
	burstLen = int(dram_config.config['burstLen'][0])
	# here we calculate different power parameter
	tot_time = float(t * 1000000000000000)
			#float(act_t + pre_t + read_t + write_t)
	ck_freq = 800
	bnk_pre = 1		#default value
	cke_lo_pre = 1	#default value
	cke_lo_act = 0	#default vaule
	wr_sch = 0.01
	rd_sch = 0.01
	rd_per = 50
	if read_t + write_t != 0:
		rd_per = round((100 * read_t) / (read_t + write_t))

	bnk_pre = float(tot_time - write_t - read_t) / tot_time
	cke_lo_pre = 0.3
	cke_lo_act = 0.3 
	wr_sch = float(write_t) / tot_time
	rd_sch = float(read_t) / tot_time
	if wr_sch + rd_sch == 0:
		wr_sch = rd_sch = 0.0001

	ck_col_avg = float(burstLen) / 2.0 / (wr_sch + rd_sch)
	ck_row_avg = ck_col_avg / (1.0 - float(page_hit_rate))
	tRRDsch = ck_row_avg * 1000 / ck_freq

	psys_pre_pdn = Idd2P * Vdd * bnk_pre * cke_lo_pre
	psys_pre_stby = Idd2N * Vdd * bnk_pre * (1 - cke_lo_pre) * ck_freq / 1000 * tCKavg
	psys_act_pdn = Idd3P * Vdd * ((1 - cke_lo_pre) * cke_lo_act) * ck_freq / 1000 * tCKavg
	psys_act_stby = Idd3N * Vdd * ((1 - cke_lo_pre) * (1 - cke_lo_act)) * ck_freq / 1000 * tCKavg
	psys_ref = (Idd5 - Idd3N) * RFC_min / REFI / 1000 * Vdd
	psys_act = (Idd0 - (Idd3N * tRAS / tRC + Idd2N * (tRC - tRAS) / tRC)) * Vdd * tRC / tRRDsch

	psys_wr = (Idd4W - Idd3N) * Vdd * burstLen / 8 * wr_sch * ck_freq / 1000 * tCKavg
	psys_rd = (Idd4W - Idd3N) * Vdd * burstLen / 8 * rd_sch * ck_freq / 1000 * tCKavg
	psys_read_io = psys_rd
#float(pt.power_table[rd_per][4])

	psys_tot_back = psys_pre_pdn + psys_pre_stby + psys_act_pdn + psys_act_stby + psys_ref + psys_act
	psys_tot_front = float(psys_wr + psys_rd + psys_read_io)
#float((1 - page_hit_rate) / 0.5) * float(psys_wr + psys_rd + psys_read_io)
	return (psys_tot_back / 1000.0, psys_tot_front / 1000.0)

def compute_dram_cntlr_power(nread, nwrite, t):
	DRAM_CLK = 800.0
	DRAM_POWER_READ = 0.678
	DRAM_POWER_WRITE = 0.825
	num_dram_controllers = 1.0
	sockets = num_dram_controllers
	chips_per_dimm = 8.0
	dimms_per_socket = 4.0
	ncycles = t * DRAM_CLK * 1e6
	read_dc = float(nread / sockets / (ncycles or 1))
	write_dc = float(nwrite / sockets / (ncycles or 1))
	power_chip_dyn = read_dc * DRAM_POWER_READ + write_dc * DRAM_POWER_WRITE
	power_socket_dyn = power_chip_dyn * chips_per_dimm * dimms_per_socket
	print power_socket_dyn, ' ', sockets
	return power_socket_dyn * sockets

core_units = [('Execution Unit', 'exe'),
			 ('Instruction Fetch Unit', 'ifetch'),
			 ('Load Store Unit', 'lsu'),
			 ('Memory Management Unit', 'mmu'),
			 ('L2', 'l2'),
			 ('Renaming Unit', 'ru')]

def main(core_stats_file, dram_stats_file, interval):
	keylist = ['L3']
	for i in xrange(4):
		keylist.append('exe_%d' % i)
		keylist.append('ifetch_%d' % i)
		keylist.append('lsu_%d' % i)
		keylist.append('mmu_%d' % i)
		keylist.append('l2_%d' % i)
		keylist.append('ru_%d' % i)
	for i in xrange(dram_config.VaultNum):
		keylist.append('dram_ctlr_%d' % i)
	for j in xrange(dram_config.BankNum):
		for i in xrange(dram_config.VaultNum):
			keylist.append('dram_%d_%d' % (i, j))
	core_trace = []
	with open(core_stats_file) as f:
		period = ''
		units = ''
		trace = ''
		for line in f:
			if line[0] == '@':
				if len(line) > 2:
					period = line[1:].strip()
				else:
					core_trace.append([period, trace])
					period = ''
					units = ''
					trace = ''
			elif line[0] != '!':
				if units == '':
					units = line.strip()
				else:
					trace = line.strip()
	with open(dram_stats_file) as f:
		period = ''
		drams = []
		dram_str = ''
		idx = 0
		for line in f: 
			if line[0] == '@':
				if len(line) > 2:
					period = line[1:].strip()
				else:
					if period[0] != 'e':
						period = ''
						dram_str = ''
						continue
					print '--------', period, 'begin:'
					period = ''
					drams = dram_str.split('\n')
					dram_str = ''
					dram_num, dram_stat_power = 0, 0
					bank_rst_list = []
					cntlr_rst_list = []
					for i in xrange(dram_config.VaultNum):
						vault_power = float(0)
						nreads = 0
						nwrites = 0
						for j in xrange(dram_config.BankNum):
							cur_bank = drams[dram_num].split()
							dram_num += 1
							bank_n = cur_bank[0]
							bank_act_t = int(cur_bank[1])
							bank_pre_t = int(cur_bank[2])
							bank_read_t = int(cur_bank[3])
							bank_write_t = int(cur_bank[4])
							bank_reads = int(cur_bank[5])
							bank_writes = int(cur_bank[6])
							bank_hits = int(cur_bank[7])
							bank_row_hit_rate = 0.99
							nreads += bank_reads
							nwrites += bank_writes
							if bank_reads + bank_writes != 0:
								bank_row_hit_rate = float(bank_hits) / float(bank_reads + bank_writes)
							cur_bank_power = compute_dram_power(bank_act_t, bank_pre_t, bank_read_t, bank_write_t, interval, bank_row_hit_rate)
							real_power = 0.0
							if bank_act_t != 0:
								bank_rst_list.append(str(cur_bank_power[0] + cur_bank_power[1]))
								real_power = cur_bank_power[0] + cur_bank_power[1]
							else:
								bank_rst_list.append(str(cur_bank_power[0]))
								real_power = cur_bank_power[0]
							vault_power += real_power
						vault_cntlr_power = compute_dram_cntlr_power(nreads, nwrites, interval)
						cntlr_rst_list.append(str(float(vault_cntlr_power)))
					bank_rst_str = '\t'.join(bank_rst_list)
					print bank_rst_str
					cntlr_rst_str = '\t'.join(cntlr_rst_list)
					core_trace[idx][1] += '\t' + cntlr_rst_str + '\t' + bank_rst_str
					idx += 1
			else:
				 dram_str += line
	output_file = dram_stats_file + '.trace'
	with open(output_file, 'w') as f:
		keystr = '\t'.join(keylist)
		f.write(keystr + '\n')
		for trace in core_trace:
			f.write(trace[1] + '\n')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]))
