#include "stats.h"
#include "simulator.h"
#include "hooks_manager.h"
#include "utils.h"
#include "itostr.h"

#include <math.h>
#include <stdio.h>
#include <sstream>
#include <unordered_set>
#include <string>
#include <cstring>
#include <cstdio>
#include <zlib.h>
#include <sys/time.h>

template <> UInt64 makeStatsValue<UInt64>(UInt64 t) { return t; }
template <> UInt64 makeStatsValue<SubsecondTime>(SubsecondTime t) { return t.getFS(); }
template <> UInt64 makeStatsValue<ComponentTime>(ComponentTime t) { return t.getElapsedTime().getFS(); }

const char* db_create_stmts[] = {
   // Statistics
   "CREATE TABLE `names` (nameid INTEGER, objectname TEXT, metricname TEXT);",
   "CREATE TABLE `prefixes` (prefixid INTEGER, prefixname TEXT);",
   "CREATE TABLE `values` (prefixid INTEGER, nameid INTEGER, core INTEGER, value INTEGER);",
   "CREATE INDEX `idx_prefix_name` ON `prefixes`(`prefixname`);",
   "CREATE INDEX `idx_value_prefix` ON `values`(`prefixid`);",
   // Other users
   "CREATE TABLE `topology` (componentname TEXT, coreid INTEGER, masterid INTEGER);",
   "CREATE TABLE `event` (event INTEGER, time INTEGER, core INTEGER, thread INTEGER, value0 INTEGER, value1 INTEGER, description TEXT);",
};
const char db_insert_stmt_name[] = "INSERT INTO `names` (nameid, objectname, metricname) VALUES (?, ?, ?);";
const char db_insert_stmt_prefix[] = "INSERT INTO `prefixes` (prefixid, prefixname) VALUES (?, ?);";
const char db_insert_stmt_value[] = "INSERT INTO `values` (prefixid, nameid, core, value) VALUES (?, ?, ?, ?);";

UInt64 getWallclockTimeCallback(String objectName, UInt32 index, String metricName, UInt64 arg)
{
   struct timeval tv = {0,0};
   gettimeofday(&tv, NULL);
   UInt64 usec = (UInt64(tv.tv_sec) * 1000000) + tv.tv_usec;
   return usec;
}

StatsManager::StatsManager()
   : m_keyid(0)
   , m_prefixnum(0)
   , m_db(NULL)
   , m_stacked_dram_unison(NULL)
   , m_stacked_dram_alloy(NULL)
   , m_stacked_dram_mem(NULL)
{
   init();

   registerMetric(new StatsMetricCallback("time", 0, "walltime", getWallclockTimeCallback, 0));
}

StatsManager::~StatsManager()
{
   if (m_stacked_dram_unison != NULL || m_stacked_dram_alloy != NULL || m_stacked_dram_mem != NULL) {
	   dram_stats_file.close();
   }
   free_dvector(unit_temp);
   free_names(unit_names);
   power_trace_log.close();
   temp_trace_log.close();

   for(StatsObjectList::iterator it1 = m_objects.begin(); it1 != m_objects.end(); ++it1)
      for (StatsMetricList::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
         for(StatsIndexList::iterator it3 = it2->second.second.begin(); it3 != it2->second.second.end(); ++it3)
            delete it3->second;

   if (m_db)
   {
      sqlite3_finalize(m_stmt_insert_name);
      sqlite3_finalize(m_stmt_insert_prefix);
      sqlite3_finalize(m_stmt_insert_value);
      sqlite3_close(m_db);
   }

   /* Dump Refresh/Access Results*/
   std::cout << "\n ***** [REF/AC_Result] *****\n";
   int tot_hot_access = 0, tot_cool_access = 0;
   for (int i = 0; i < 32; i++) {
	   for (int j = 0; j < 8; j++) {
		   //std::cout << "/*BANK*/" << i << "->" << j
			//		 << ": cool access (" << cool_access[i][j] << "), "
			//		 << "hot access (" << hot_access[i][j] << "), " 
			//		 << "error acces (" << err_access[i][j] << ")." << std::endl;
		   tot_hot_access += hot_access[i][j];
		   tot_cool_access += cool_access[i][j];
	   }
   }
   std::cout << "{Summary}: " << tot_hot_access << " Hot Accesses, "
			 << tot_cool_access << " Cool Accesses." << std::endl;
   std::cout << "\n ***** [REF/AC_Result] *****\n";
	
}

void
StatsManager::init_stacked_dram_unison(StackedDramPerfUnison *stacked_dram)
{
	std::cout << "now initialize a stacked dram unison cache" << std::endl;
	m_stacked_dram_unison = stacked_dram;
	dram_stats_file.open("unison_cache.stats");
}

void
StatsManager::init_stacked_dram_alloy(StackedDramPerfAlloy *stacked_dram)
{
	std::cout << "now initialize a stacked dram alloy cache" << std::endl;
	m_stacked_dram_alloy = stacked_dram;
	dram_stats_file.open("alloy_cache.stats");
}

void
StatsManager::init_stacked_dram_mem(StackedDramPerfMem *stacked_dram)
{
	std::cout << "now initialize a stacked dram mem" << std::endl;
	m_stacked_dram_mem = stacked_dram;
	dram_stats_file.open("mem.stats");
}

void
StatsManager::init()
{
	/*Begin Init*/
	printf("----------BEGIN_INIT_STATS_MANAGER------------\n");
	/**/
   String filename = Sim()->getConfig()->formatOutputFileName("sim.stats.sqlite3");
   int ret;

   unlink(filename.c_str());
   ret = sqlite3_open(filename.c_str(), &m_db);
   LOG_ASSERT_ERROR(ret == SQLITE_OK, "Cannot create DB");
   sqlite3_exec(m_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
   sqlite3_exec(m_db, "PRAGMA journal_mode = MEMORY", NULL, NULL, NULL);
   sqlite3_busy_handler(m_db, __busy_handler, this);

   for(unsigned int i = 0; i < sizeof(db_create_stmts)/sizeof(db_create_stmts[0]); ++i)
   {
      int res; char* err;
      res = sqlite3_exec(m_db, db_create_stmts[i], NULL, NULL, &err);
      LOG_ASSERT_ERROR(res == SQLITE_OK, "Error executing SQL statement \"%s\": %s", db_create_stmts[i], err);
   }

   sqlite3_prepare(m_db, db_insert_stmt_name, -1, &m_stmt_insert_name, NULL);
   sqlite3_prepare(m_db, db_insert_stmt_prefix, -1, &m_stmt_insert_prefix, NULL);
   sqlite3_prepare(m_db, db_insert_stmt_value, -1, &m_stmt_insert_value, NULL);

   sqlite3_exec(m_db, "BEGIN TRANSACTION", NULL, NULL, NULL);
   for(StatsObjectList::iterator it1 = m_objects.begin(); it1 != m_objects.end(); ++it1)
   {
      for (StatsMetricList::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
      {
		  /*Output metric names*/
		  printf("---------OUTPUT_METRIC----------------\n");
		  printf("keyID: %ld, ObjectName: %s, MetricName: %s\n", it2->second.first, it1->first.c_str(), it2->first.c_str());
		  printf("---------END_OUTPUT-------------------\n");
		  

         recordMetricName(it2->second.first, it1->first, it2->first);
      }
   }
   sqlite3_exec(m_db, "END TRANSACTION", NULL, NULL, NULL);

   /*Initialize log file*/
   power_trace_log.open("power_trace_log.txt");
	for (int i = 0; i < 4; i++) {
		power_trace_log << "ialu_" << i << "\t";
		power_trace_log << "fpalu_" << i << "\t";
		power_trace_log << "inssch_" << i << "\t";
		power_trace_log << "l1i_" << i << "\t";
		power_trace_log << "insdec_" << i << "\t";
		power_trace_log << "bp_" << i << "\t";
		power_trace_log << "ru_" << i << "\t";
		power_trace_log << "l1d_" << i << "\t";
		power_trace_log << "mmu_" << i << "\t";
		power_trace_log << "l2_" << i << "\t";
	}

	// 2.5D test
	power_trace_log << "airgap0\t";

	for (int i = 0; i < 32; i++) {
		power_trace_log << "dram_ctlr_" << i << "\t";
	}
	for (int j = 0; j < 8; j++) {
		// 2.5D test
		if (j % 2 == 0) {
			int air_gap_num = j / 2;
			if (air_gap_num == 0) {
				power_trace_log << "airgap1\t";
			} else if (air_gap_num == 1) {
				power_trace_log << "airgap2\t";
			} else if (air_gap_num == 2) {
				power_trace_log << "airgap3\t";
			} else if (air_gap_num == 3) {
				power_trace_log << "airgap4\t";
			}
		}

		for (int i = 0; i < 32; i++) {
			power_trace_log << "dram_" << i << "_" << j << "\t";
		}
	}
	power_trace_log << std::endl;

   temp_trace_log.open(ttrace_file);

   /* Set whether to dump trace*/
   dump_trace = Sim()->getCfg()->getBoolDefault("perf_model/thermal/dump_trace", false);

   ttrace_num = 0;
   /*Initial Hotspot*/
   int hotspot_analysis_threshold = Sim()->getCfg()->getInt("perf_model/thermal/hotspot_analysis_threshold");
   hotspot = new Hotspot();
   unit_names = alloc_names(MAX_UNITS, STR_SIZE);
   unit_temp = dvector(MAX_UNITS);
   /*Get Unit names*/
   unit_num = 0;
   hotspot->getNames("./HotSpot/powertrace.input", unit_names, &unit_num);
   hotspot->_start_analysis_threshold = double(hotspot_analysis_threshold);
   /*Initial DRAM bank statistics*/
   for (int i = 0; i < 32; i++) {
	   vault_reads[i] = 0;
	   vault_writes[i] = 0;
	   vault_row_hits[i] = 0;
	   for (int j =0; j < 8; j++) {
		   bank_stats[i][j].tACT = SubsecondTime::Zero();
		   bank_stats[i][j].tPRE = SubsecondTime::Zero();
		   bank_stats[i][j].tRD = SubsecondTime::Zero();
		   bank_stats[i][j].tWR = SubsecondTime::Zero();
		   bank_stats[i][j].reads = 0;
		   bank_stats[i][j].writes = 0;
		   bank_stats[i][j].row_hits = 0;

		   hot_access[i][j] = 0;
		   cool_access[i][j] = 0;
		   err_access[i][j] = 0;

		   prev_bank_temp[i][j] = 0;
	   }
   }
   /*Initialize power values*/
	for (int i = 0; i < 4; i++) {
		power_ialu[i] = power_fpalu[i] = power_inssch[i] 
					  = power_l1i[i] = power_insdec[i] 
					  = power_bp[i] = power_ru[i] 
					  = power_l1d[i] = power_mmu[i]
					  = power_l2[i] = 0;
		p_power_ialu[i] = p_power_fpalu[i] = p_power_inssch[i] 
					  = p_power_l1i[i] = p_power_insdec[i] 
					  = p_power_bp[i] = p_power_ru[i] 
					  = p_power_l1d[i] = p_power_mmu[i]
					  = p_power_l2[i] = 0;
	}
	for (int i = 0; i < 32; i++) {
		vault_power[i] = 0;
	}
	for (int j = 0; j < 8; j++) {
		for (int i =0; i < 32; i++) {
			bank_power[i][j] = 0;
		}
	}
   /*Initialize current time*/
   m_current_time = SubsecondTime::Zero();
   m_last_remap_time = SubsecondTime::Zero();
   m_last_record_time = SubsecondTime::Zero();
   m_record_interval = SubsecondTime::Zero();

   last_time_point = std::chrono::steady_clock::now();

   /* Initialize remap interval*/
	// EXP_SET
   int remap_interval_us = Sim()->getCfg()->getInt("perf_model/remap_config/remap_interval");
   RemapInterval = SubsecondTime::US(remap_interval_us);
   do_remap = Sim()->getCfg()->getBoolDefault("perf_model/remap_config/remap", false);
   reverse_flp = Sim()->getCfg()->getBoolDefault("perf_model/thermal/reverse", false);

   /* Initialization of frequency table for DVFS*/
   int max_freq = Sim()->getCfg()->getFloat("perf_model/core/frequency") * 1000;
   int min_freq = max_freq * 0.2;
   int num_freq = Sim()->getCfg()->getInt("perf_model/thermal/freq_num");
   int freq_step = (max_freq - min_freq) / (num_freq - 1);
   for (int i = 0; i < num_freq - 1; i++) {
	   freq_table.push_back(min_freq + freq_step * i);
   }
   freq_table.push_back(max_freq);
   freq_lev = freq_table.size() - 1;
   max_lev = freq_table.size() - 1;

   first_ttrace = false;
   start_hotspot = false;

	/*End Init*/
	printf("----------END_INIT_STATS_MANAGER------------\n");
	/**/
}

int
StatsManager::busy_handler(int count)
{
   // With a usleep below of 10 ms, at most one warning every 10s
   if (count % 1000 == 999)
   {
      LOG_PRINT_WARNING("Difficulty locking sim.stats.sqlite3, retrying...");
   }
   usleep(10000);
   return 1;
}

void
StatsManager::recordMetricName(UInt64 keyId, std::string objectName, std::string metricName)
{
   int res;
   sqlite3_reset(m_stmt_insert_name);
   sqlite3_bind_int(m_stmt_insert_name, 1, keyId);
   sqlite3_bind_text(m_stmt_insert_name, 2, objectName.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(m_stmt_insert_name, 3, metricName.c_str(), -1, SQLITE_TRANSIENT);
   res = sqlite3_step(m_stmt_insert_name);
   LOG_ASSERT_ERROR(res == SQLITE_DONE, "Error executing SQL statement");
}

void
StatsManager::checkDTM(const vector<double> cpu_temp, double dram_temp)
{
	double cpu_temp_thres = Sim()->getCfg()->getInt("perf_model/thermal/cpu_temp_thres"), 
		   dram_temp_thres = Sim()->getCfg()->getInt("perf_model/thermal/dram_temp_thres");
	double max_temp = 0;
	int control_method = Sim()->getCfg()->getInt("perf_model/thermal/dtm_method");
	for (UInt32 i = 0; i < cpu_temp.size(); i++) {
		if (cpu_temp[i] > max_temp) max_temp = cpu_temp[i];
	}
	printf("[DTM Trigger] CPU_MAX(THRES): %.3lf(%.3lf), DRAM_MAX(THRES): %.3lf(%.3lf)\n\tCurrent DTM method: %d\n", 
			max_temp, cpu_temp_thres, dram_temp, dram_temp_thres, control_method);

	if (control_method == 0) {
		// 0: No operation on high temperature
		return;
	} else if (control_method == 1) {
		// 1: DVFS considering only CPU temperature
		if (max_temp > cpu_temp_thres) {
			std::cout << "[HOT!] CPU is too hot!\n";
			if (freq_lev > 0) {
				freq_lev--;
				setGlobalFrequency(freq_table[freq_lev]);
			}
		} else {
			if (freq_lev < max_lev) {
				freq_lev++;
				setGlobalFrequency(freq_table[freq_lev]);
			}
		}
	} else if (control_method == 2) {
		// 2: DVFS considering both CPU and DRAM temperature
		
		if (max_temp > cpu_temp_thres || dram_temp > dram_temp_thres) {
			if (max_temp > cpu_temp_thres) {
				std::cout << "[HOT!] CPU is too hot!\n";
			} else {
				std::cout << "[HOT!] DRAM is too hot!\n";
			}
			if (freq_lev > 0) {
				freq_lev--;
				setGlobalFrequency(freq_table[freq_lev]);
			}
		} else {
			if (freq_lev < max_lev) {
				freq_lev++;
				setGlobalFrequency(freq_table[freq_lev]);
			}
		}
	} else {
		std::cout << "[DTM Trigger] Unrecognized DTM method!\n";
	}
}

void
StatsManager::setGlobalFrequency(UInt64 freq_in_mhz)
{
	UInt32 num_cores = Sim()->getConfig()->getApplicationCores();
	for (UInt32 c_i = 0; c_i < num_cores; c_i++) {
		setFrequency(c_i, freq_in_mhz);
	}
}

void
StatsManager::setFrequency(UInt64 core_num, UInt64 freq_in_mhz)
{
	std::cout << "[SNIPER_STATS_MANAGER] Setting frequency for core " << core_num
			  << " in DVFS domain " << Sim()->getDvfsManager()->getCoreDomainId(core_num)
			  << " to " << freq_in_mhz << " MHz" << std::endl;
	UInt64 freq_in_hz = freq_in_mhz * 1000000;
	if (freq_in_hz > 0) {
		Sim()->getDvfsManager()->setCoreDomain(core_num, ComponentPeriod::fromFreqHz(freq_in_hz));
	} else {
		std::cout << "[SNIPER_STATS_MANAGER] Fail to set frequency" << std::endl;
	}
}

double
StatsManager::computeBankPower(double bnk_pre, double cke_lo_pre, double page_hit, double WRsch, double RDsch, bool hot, double Vdd_use)
{
	double rd_wr_pre = WRsch + RDsch, rev_page_hit = (1 - page_hit);
	
	double tRRDsch;
	if (rd_wr_pre != 0 && rev_page_hit != 0) 
		tRRDsch = (dram_table.burstLen / 2.0 / (WRsch + RDsch)) / (1 - page_hit);
	else
		tRRDsch = dram_table.tRC;
	tRRDsch = tRRDsch * 1000 / dram_table.freq;
	double psch_pre_pdn = dram_table.Idd2P * dram_table.Vdd * bnk_pre * cke_lo_pre;
	double psch_pre_stby = dram_table.Idd2N * dram_table.Vdd * bnk_pre * (1 - cke_lo_pre);
	double psch_act_pdn = dram_table.Idd3P * dram_table.Vdd * (1 - bnk_pre) * cke_lo_pre;
	double psch_act_stby = dram_table.Idd3N * dram_table.Vdd * (1 - bnk_pre) * (1 - cke_lo_pre);
	double pds_act = (dram_table.Idd0 - (dram_table.Idd3N * dram_table.tRAS + dram_table.Idd2N * (dram_table.tRC - dram_table.tRAS)) / dram_table.tRC) * dram_table.Vdd;
	double psch_act = pds_act * dram_table.tRC / tRRDsch;
	double psch_WR = (dram_table.Idd4W - dram_table.Idd3N) * dram_table.Vdd * WRsch;
	double psch_RD = (dram_table.Idd4R - dram_table.Idd3N) * dram_table.Vdd * RDsch;
	double psch_REF = (dram_table.Idd5 - dram_table.Idd3N) * dram_table.Vdd * dram_table.RFC_min / dram_table.REFI;
	if (hot) psch_REF *= 2;

	double psys = psch_pre_pdn + psch_pre_stby + psch_act_pdn + psch_act_stby + psch_act + psch_WR + psch_RD + psch_REF;
	// Scale to real V
	psys = psys * (Vdd_use / dram_table.Vdd) * (Vdd_use / dram_table.Vdd);
	
	//std::cout << "[ComputeBankPower] bnk_pre: " << bnk_pre << ", cke_lo_pre: " << cke_lo_pre << ", page_hit_rate: " << page_hit << ", WRsch: " << WRsch << ", RDsch: " << RDsch << std::endl;
	//std::cout << "**tRRDsch: " << tRRDsch << ", Power: " << psys / 1000.0 << std::endl;
	//std::cout << "[Detailed] background: " << psch_pre_pdn + psch_pre_stby + psch_act_pdn + psch_act_stby << ", psch_act: " << psch_act << ", psch_WR: " << psch_WR << ", psch_RD: " << psch_RD << ", psch_ref: " << psch_REF << std::endl;
	return psys / 1000.0;
}

double 
StatsManager::computeDramPower(SubsecondTime tACT, SubsecondTime tPRE, SubsecondTime tRD, SubsecondTime tWR, SubsecondTime totT, UInt32 reads, UInt32 writes, double page_hit_rate)
{
	double act_t = double(tACT.getFS()), pre_t = double(tPRE.getFS()), read_t = double(tRD.getFS()), write_t = double(tWR.getFS()), tot_time = double(totT.getFS());
	if (act_t > tot_time) {
		//std::cout << "[Potetial Error] tACT > tTOT\n";
		act_t = tot_time;
		if (pre_t > act_t)
			pre_t = act_t;
	}
	double read_ratio = 0,
		   write_ratio = 0;
	if (reads + writes != 0) {
		read_ratio = double(reads) / double(reads + writes);
		write_ratio = double(writes) / double(reads + writes);
	}
	read_t = read_ratio * act_t;
	write_t = write_ratio * act_t;
    /*set some default value*/
	int ck_freq = 800;
    double bnk_pre = 1;
    double cke_lo_pre = 1;
    double cke_lo_act = 0;
    double wr_sch = 0.01;
    double rd_sch = 0.01;

	/*set the real value*/
	bnk_pre = double((tot_time - act_t) / tot_time);
	cke_lo_pre = 0.3;
	cke_lo_act = 0.3;
	wr_sch = write_t / tot_time;
	rd_sch = read_t / tot_time;
	if (wr_sch + rd_sch == 0)
		wr_sch = rd_sch = 0.0001;

	double ck_col_avg = double(dram_table.burstLen) / 2.0 / (wr_sch + rd_sch);
	double ck_row_avg = ck_col_avg / (1.0 - double(page_hit_rate));
	double tRRDsch = ck_row_avg * 1000 / ck_freq;

	double psys_pre_pdn = dram_table.Idd2P * dram_table.Vdd * bnk_pre * cke_lo_pre;
	double psys_pre_stby = dram_table.Idd2N * dram_table.Vdd * bnk_pre * (1 - cke_lo_pre) * ck_freq / 1000 * dram_table.tCKavg;
	double psys_act_pdn = dram_table.Idd3P * dram_table.Vdd * ((1 - cke_lo_pre) * cke_lo_act) * ck_freq / 1000 * dram_table.tCKavg;
	double psys_act_stby = dram_table.Idd3N * dram_table.Vdd * ((1 - cke_lo_pre) * (1 - cke_lo_act)) * ck_freq / 1000 * dram_table.tCKavg;
	double psys_ref = (dram_table.Idd5 - dram_table.Idd3N) * dram_table.RFC_min / dram_table.REFI / 1000 * dram_table.Vdd;
	double psys_act = (dram_table.Idd0 - (dram_table.Idd3N * dram_table.tRAS / dram_table.tRC + dram_table.Idd2N * (dram_table.tRC - dram_table.tRAS) / dram_table.tRC)) * dram_table.Vdd * dram_table.tRC / tRRDsch;

	double psys_wr = (dram_table.Idd4W - dram_table.Idd3N) * dram_table.Vdd * dram_table.burstLen / 8 * wr_sch * ck_freq / 1000 * dram_table.tCKavg;
	double psys_rd = (dram_table.Idd4W - dram_table.Idd3N) * dram_table.Vdd * dram_table.burstLen / 8 * rd_sch * ck_freq / 1000 * dram_table.tCKavg;
	double psys_read_io = psys_rd;

	double psys_tot_back = psys_pre_pdn + psys_pre_stby + psys_act_pdn + psys_act_stby + psys_ref + psys_act;
	double psys_tot_front = double(psys_wr + psys_rd + psys_read_io);
	
	return (psys_tot_back / 1000.0 +  psys_tot_front / 1000.0);
}

double
StatsManager::computeDramCntlrPower(UInt32 reads, UInt32 writes, SubsecondTime t, UInt32 tot_access)
{
	double tot_time = double(t.getFS()) * 1.0e-15;
	double ncycles = tot_time * dram_cntlr_table.DRAM_CLK * 1e6;
	double sockets = dram_cntlr_table.num_dram_controllers;
	double read_dc, write_dc;
	if (ncycles != 0) {
		read_dc = double(reads / sockets / ncycles);
		write_dc = double(writes / sockets / ncycles);
	}
	else {
		read_dc = double(reads / sockets);
		write_dc = double(writes / sockets);
	}
	double power_chip_dyn = read_dc * dram_cntlr_table.DRAM_POWER_READ + write_dc * dram_cntlr_table.DRAM_POWER_WRITE;
	double power_socket_dyn = power_chip_dyn * dram_cntlr_table.chips_per_dimm * dram_cntlr_table.dimms_per_socket;
	double raw_power = power_socket_dyn * sockets;
	double ratio = 0;
	if (tot_access != 0)
		ratio = double(reads + writes) / double(tot_access);
	if (ratio > 1) ratio = 1;
	double real_power = power_mc * ratio;
	if (raw_power > real_power)
		raw_power = real_power;
	
	return real_power;
}

void
StatsManager::dumpDramPowerTrace()
{
// output stacked dram stats
   /*
	Unison Cache Stats
	  */
#ifdef OUTPUT_DRAM_STAT
   if (m_stacked_dram_unison != NULL) {
	   //std::cout << "Stacked dram cache has " << m_stacked_dram_unison->n_vaults << " vaults!" << std::endl;

		SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
		tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

		UInt32 tot_access, tot_row_hits;

		tot_access = tot_row_hits = 0;

		for (UInt32 i = 0; i < m_stacked_dram_unison->n_vaults; i++) {
			VaultPerfModel* vault = m_stacked_dram_unison->m_vaults_array[i];
			for (UInt32 j = 0; j < vault->n_banks; j++) {
				BankPerfModel* bank = vault->m_banks_array[j];
				dram_stats_file << "dram_" << i << "_" << j << " " 
						<< bank->stats.tACT << " " 
						<< bank->stats.tPRE << " " 
						<< bank->stats.tRD << " " 
						<< bank->stats.tWR << " "
						<< bank->stats.reads << " "
						<< bank->stats.writes << " "
						<< bank->stats.row_hits << std::endl;

				tot_ACT += bank->stats.tACT;
				tot_PRE += bank->stats.tPRE;
				tot_RD += bank->stats.tRD;
				tot_WR += bank->stats.tWR;

				tot_access += bank->stats.reads + bank->stats.writes;
				tot_row_hits += bank->stats.row_hits;
			}
		}
		float row_hit_rate = 0;
		if (tot_access != 0) 
			row_hit_rate = (float)tot_row_hits / (float)tot_access;

		//dram_stats_file << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
		//dram_stats_file << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
		//dram_stats_file << "@" << std::endl;
   }

   /*
	Alloy Cache Stats
	  */
   if (m_stacked_dram_alloy != NULL) {
	   //std::cout << "Stacked dram cache has " << m_stacked_dram_alloy->n_vaults << " vaults!" << std::endl;

		SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
		tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

		UInt32 tot_access, tot_row_hits;

		tot_access = tot_row_hits = 0;

	    dram_stats_file << "@" <<  prefix << std::endl;
		for (UInt32 i = 0; i < m_stacked_dram_alloy->n_vaults; i++) {
			VaultPerfModel* vault = m_stacked_dram_alloy->m_vaults_array[i];
			for (UInt32 j = 0; j < vault->n_banks; j++) {
				BankPerfModel* bank = vault->m_banks_array[j];
				dram_stats_file << "dram_" << i << "_" << j << " " 
						<< bank->stats.tACT << " " 
						<< bank->stats.tPRE << " " 
						<< bank->stats.tRD << " " 
						<< bank->stats.tWR << " "
						<< bank->stats.reads << " "
						<< bank->stats.writes << " "
						<< bank->stats.row_hits << std::endl;

				tot_ACT += bank->stats.tACT;
				tot_PRE += bank->stats.tPRE;
				tot_RD += bank->stats.tRD;
				tot_WR += bank->stats.tWR;

				tot_access += bank->stats.reads + bank->stats.writes;
				tot_row_hits += bank->stats.row_hits;
			}
		}
		float row_hit_rate = 0;
		if (tot_access != 0) 
			row_hit_rate = (float)tot_row_hits / (float)tot_access;

		//dram_stats_file << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
		//dram_stats_file << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
		//dram_stats_file << "@" << std::endl;
   }

   /*
	PoM Stats
	  */
   if (m_stacked_dram_mem != NULL) {
	   //std::cout << "Stacked dram mem has " << m_stacked_dram_mem->n_vaults << " vaults!" << std::endl;
		SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
		tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

		UInt32 tot_access, tot_row_hits;

		tot_access = tot_row_hits = 0;

	    dram_stats_file << "@" <<  prefix << std::endl;
		for (UInt32 i = 0; i < m_stacked_dram_mem->n_vaults; i++) {
			VaultPerfModel* vault = m_stacked_dram_mem->m_vaults_array[i];
			for (UInt32 j = 0; j < vault->n_banks; j++) {
				BankPerfModel* bank = vault->m_banks_array[j];
				dram_stats_file << "dram_" << i << "_" << j << " " 
						<< bank->stats.tACT << " " 
						<< bank->stats.tPRE << " " 
						<< bank->stats.tRD << " " 
						<< bank->stats.tWR << " "
						<< bank->stats.reads << " "
						<< bank->stats.writes << " "
						<< bank->stats.row_hits << std::endl;

				tot_ACT += bank->stats.tACT;
				tot_PRE += bank->stats.tPRE;
				tot_RD += bank->stats.tRD;
				tot_WR += bank->stats.tWR;

				tot_access += bank->stats.reads + bank->stats.writes;
				tot_row_hits += bank->stats.row_hits;
			}
		}
		float row_hit_rate = 0;
		if (tot_access != 0) 
			row_hit_rate = (float)tot_row_hits / (float)tot_access;

		//dram_stats_file << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
		//dram_stats_file << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
		//dram_stats_file << "@" << std::endl;
   }
#endif
}

void
StatsManager::updateBankStat(int i, int j, BankPerfModel* bank)
{
	bank_stats[i][j] = bank->stats;
}

void
StatsManager::recordPowerTrace()
{
}

void
StatsManager::updatePower()
{
	// Store the current power to previous array
	int power_scale_int = Sim()->getCfg()->getInt("perf_model/thermal/power_scale");
	double power_scale = double(power_scale_int) / 10.0;
	if (power_scale < 1) {
		std::cout << "[Warning] power scale is less than 1, please check!\n";
		power_scale = 1;
	}


	peak_power_proc = double(getMetricObject("peak_processor", 0, "power-dynamic")->recordMetric()) * 1.0e-6;
	dyn_power_proc = double(getMetricObject("processor", 0, "power-dynamic")->recordMetric()) * 1.0e-6;
	if (power_scale_int == -1) {
		if (dyn_power_proc != 0) {
			power_scale = peak_power_proc / dyn_power_proc;
		} else {
			power_scale = 1;
		}
		std::cout << "[Overall Power Consumption] Peak: " << peak_power_proc 
				  << ", Dynamic: " << dyn_power_proc << std::endl;
	}

	p_power_mc = power_mc;
	for (int i = 0; i < 4; i++) {
		p_power_exe[i] = power_exe[i];
		p_power_ifetch[i] = power_ifetch[i];
		p_power_lsu[i] = power_lsu[i];
		p_power_mmu[i] = power_mmu[i];
		p_power_l2[i] = power_l2[i];
		p_power_ru[i] = power_ru[i];
		p_power_ialu[i] = power_ialu[i];
		p_power_fpalu[i] = power_fpalu[i];
		p_power_inssch[i] = power_inssch[i];
		p_power_l1i[i] = power_l1i[i];
		p_power_insdec[i] = power_insdec[i];
		p_power_bp[i] = power_bp[i];
		p_power_l1d[i] = power_l1d[i];
	}

	power_mc = power_scale * double(getMetricObject("mc", 0, "power-dynamic")->recordMetric()) * 1.0e-6;
	for (int i = 0; i < 4; i++) {
		power_exe[i] = power_scale * double(getMetricObject("exe", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_ifetch[i] = power_scale * double(getMetricObject("ifetch", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_lsu[i] = power_scale * double(getMetricObject("lsu", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_mmu[i] = power_scale * double(getMetricObject("mmu", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_l2[i] = power_scale * double(getMetricObject("l2", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_ru[i] = power_scale * double(getMetricObject("ru", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_ialu[i] = power_scale * double(getMetricObject("ialu", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_fpalu[i] = power_scale * double(getMetricObject("fpalu", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_inssch[i] = power_scale * double(getMetricObject("inssch", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_l1i[i] = power_scale * double(getMetricObject("l1i", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_insdec[i] = power_scale * double(getMetricObject("insdec", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_bp[i] = power_scale * (double(getMetricObject("btb", i, "power-dynamic")->recordMetric()) + double(getMetricObject("btb", i, "power-dynamic")->recordMetric())) * 1.0e-6;
		power_l1d[i] = power_scale * double(getMetricObject("l1d", i, "power-dynamic")->recordMetric()) * 1.0e-6;
	}

	/* Set of time interval*/
	SubsecondTime tot_time = m_record_interval;

//	UInt32 n_banks = 8;
//	UInt32 n_vaults = m_stacked_dram_unison->n_vaults;
	UInt32 v_r[32], v_w[32];
	UInt32 tot_access = 0;

   if (m_stacked_dram_unison != NULL) {

		for (UInt32 i = 0; i < m_stacked_dram_unison->n_vaults; i++) {
			//double std_power = 0;
			VaultPerfModel* vault = m_stacked_dram_unison->m_vaults_array[i];
			//n_banks = vault->n_banks;

			UInt32 tot_reads, tot_writes;
			tot_reads = vault->stats.reads - vault_reads[i];
			tot_writes = vault->stats.writes - vault_writes[i];
			//tot_row_hits = vault->stats.row_hits - vault_row_hits[i];
			v_r[i] = tot_reads;
			v_w[i] = tot_writes;

			// Update Vault Stats
			vault_reads[i] = vault->stats.reads;
			vault_writes[i] = vault->stats.writes;
			vault_row_hits[i] = vault->stats.row_hits;

			for (UInt32 j = 0; j < vault->n_banks; j++) {
				BankPerfModel* bank = vault->m_banks_array[j];
				bank_stats_interval[i][j] = bank->stats;
				bank_stats_interval[i][j].tACT -= bank_stats[i][j].tACT;
				bank_stats_interval[i][j].tPRE -= bank_stats[i][j].tPRE;
				bank_stats_interval[i][j].reads -= bank_stats[i][j].reads;
				bank_stats_interval[i][j].writes -= bank_stats[i][j].writes;
				bank_stats_interval[i][j].row_hits -= bank_stats[i][j].row_hits;
				bank_stats_interval[i][j].row_conflicts -= bank_stats[i][j].row_conflicts;
				bank_stats_interval[i][j].row_misses -= bank_stats[i][j].row_misses;

				bank_stats_interval[i][j].tRD = SubsecondTime::Zero();
				bank_stats_interval[i][j].tWR = SubsecondTime::Zero();
				bank_stats_interval[i][j].hot = bank_stats[i][j].hot;

				/*Update current bank statistics*/
				updateBankStat(i, j, bank);

				BankStatEntry *tmp = &bank_stats_interval[i][j];

				double page_hit_rate = 0;
				if (tmp->row_hits + tmp->row_misses + tmp->row_conflicts > 0) {
					page_hit_rate = tmp->row_hits / (tmp->row_hits + tmp->row_misses + tmp->row_conflicts);
				}
				double bnk_pre, cke_lo_pre = 0.3, wr_sch, rd_sch, vdd_use = 1.5;
				bool hot = tmp->hot;
				if (tot_time.getNS() != 0) {
					bnk_pre = (1 - double(tmp->tACT.getNS())/double(tot_time.getNS()));
					if (bnk_pre < 0) bnk_pre = 0;
				} else {
					bnk_pre = 0;
				}

				if (tmp->reads + tmp->writes > 0) {
					wr_sch = double(tmp->writes) / double(tmp->reads + tmp->writes) * (1 - bnk_pre);
					rd_sch = double(tmp->reads) / double(tmp->reads + tmp->writes) * (1 - bnk_pre);
				} else {
					wr_sch = 0;
					rd_sch = 0;
				}
				bank_power[i][j] = computeBankPower(bnk_pre, cke_lo_pre, page_hit_rate, wr_sch, rd_sch, hot, vdd_use);
			}
			//vault_power[i] = computeDramCntlrPower(tot_reads, tot_writes, tot_time);
			vault_access[i] = tot_reads + tot_writes;
			tot_access += vault_access[i];
		}
   }

   for (UInt32 i = 0; i < m_stacked_dram_unison->n_vaults; i++) {
	   vault_power[i] = computeDramCntlrPower(v_r[i], v_w[i], tot_time, tot_access);
   }
/*
   Here we can choose to measure the results of 2.5D model
   In which, we turn off the power of CPU components
 */
#ifdef __ONLY_DRAM__
	/*
	power_L3 = 0;
	for (int i = 0; i < 4; i++) {
		power_exe[i] = 0;
		power_ifetch[i] = 0;
		power_lsu[i] = 0;
		power_mmu[i] = 0;
		power_l2[i] = 0;
		power_ru[i] = 0;
	}
	*/
#endif
}

void
StatsManager::dumpHotspotInput()
{
	//std::cout << "[STAT_DEBUG] Begin Dumping HotspotInput\n" << std::endl;
	/* Here we dump input for hotspot
	 * init_file(tmp.steady) from last time
	 * -p powertrace.input
	 */
	std::ofstream pt_file;
	pt_file.open("./HotSpot/powertrace.input");
	for (int i = 0; i < 4; i++) {
		pt_file << "ialu_" << i << "\t";
		pt_file << "fpalu_" << i << "\t";
		pt_file << "inssch_" << i << "\t";
		pt_file << "l1i_" << i << "\t";
		pt_file << "insdec_" << i << "\t";
		pt_file << "bp_" << i << "\t";
		pt_file << "ru_" << i << "\t";
		pt_file << "l1d_" << i << "\t";
		pt_file << "mmu_" << i << "\t";
		pt_file << "l2_" << i << "\t";
	}
	for (int i = 0; i < 32; i++) {
		pt_file << "dram_ctlr_" << i << "\t";
	}
	for (int j = 0; j < 8; j++) {
		for (int i = 0; i < 32; i++) {
			pt_file << "dram_" << i << "_" << j << "\t";
		}
	}
	pt_file << std::endl;
	
	bool record_power = Sim()->getCfg()->getBoolDefault("perf_model/thermal/record_power", false);

	if (record_power && peak_power_proc > 0.1) {
		for (int i = 0; i < 4; i++) {
			power_trace_log
			<< power_ialu[i] << "\t" 
			<< power_fpalu[i] << "\t" 
			<< power_inssch[i] << "\t" 
			<< power_l1i[i] << "\t" 
			<< power_insdec[i] << "\t" 
			<< power_bp[i] << "\t" 
			<< power_ru[i] << "\t" 
			<< power_l1d[i] << "\t" 
			<< power_mmu[i] << "\t" 
			<< power_l2[i] << "\t";
		}
		power_trace_log << "0\t";
		for (int i = 0; i < 32; i++) {
			power_trace_log << vault_power[i] << "\t";
		}
		for (int j = 0; j < 8; j++) {
			if (j % 2 == 0) {
				power_trace_log << "0\t";
			}
			for (int i =0; i < 32; i++) {
				power_trace_log << bank_power[i][j] << "\t";
			}
		}
		power_trace_log << std::endl;
	}
	/* Here we update the power data*/
	updatePower();

	int pt_num = Sim()->getCfg()->getInt("perf_model/thermal/pt_num");
	/* Here we output the second trace of HotSpot Input*/
	for (int pt = 0; pt <= pt_num; pt ++) {
		double p_ratio = double(pt) / double(pt_num);
		p_ratio = 1;
		for (int i = 0; i < 4; i++) {
			pt_file 
			<< p_power_ialu[i] + p_ratio * (power_ialu[i] - p_power_ialu[i]) << "\t" 
			<< p_power_fpalu[i] + p_ratio * (power_fpalu[i] - p_power_fpalu[i]) << "\t" 
			<< p_power_inssch[i] + p_ratio * (power_inssch[i] - p_power_inssch[i]) << "\t" 
			<< p_power_l1i[i] + p_ratio * (power_inssch[i] - p_power_inssch[i]) << "\t" 
			<< p_power_insdec[i] + p_ratio * (power_insdec[i] - p_power_insdec[i]) << "\t" 
			<< p_power_bp[i] + p_ratio * (power_bp[i] - p_power_bp[i]) << "\t" 
			<< p_power_ru[i] + p_ratio * (power_ru[i] - p_power_ru[i]) << "\t" 
			<< p_power_l1d[i] + p_ratio * (power_l1d[i] - p_power_l1d[i]) << "\t" 
			<< p_power_mmu[i] + p_ratio * (power_mmu[i] - p_power_mmu[i]) << "\t" 
			<< p_power_l2[i] + p_ratio * (power_l2[i] - p_power_l2[i]) << "\t";
		}
		for (int i = 0; i < 32; i++) {
			pt_file << vault_power[i] << "\t";
		}
		for (int j = 0; j < 8; j++) {
			for (int i =0; i < 32; i++) {
				pt_file << bank_power[i][j] << "\t";
			}
		}
		pt_file << std::endl;
	}

	pt_file.close();
}

void
StatsManager::dumpHotspotInputReverse()
{
	/* Here we dump input for hotspot
	 * init_file(tmp.steady) from last time
	 * -p powertrace.input
	 */

/* Write down power trace*/
	std::ofstream pt_file;
	pt_file.open("./HotSpot/powertrace.input");
	for (int j = 0; j < 8; j++) {
		for (int i = 0; i < 32; i++) {
			pt_file << "dram_" << i << "_" << j << "\t";
		}
	}
	for (int i = 0; i < 32; i++) {
		pt_file << "dram_ctlr_" << i << "\t";
	}
	//pt_file << "L3";
	for (int i = 0; i < 4; i++) {
		pt_file << "ialu_" << i << "\t";
		pt_file << "fpalu_" << i << "\t";
		pt_file << "inssch_" << i << "\t";
		pt_file << "l1i_" << i << "\t";
		pt_file << "insdec_" << i << "\t";
		pt_file << "bp_" << i << "\t";
		pt_file << "ru_" << i << "\t";
		pt_file << "l1d_" << i << "\t";
		pt_file << "mmu_" << i << "\t";
		pt_file << "l2_" << i << "\t";
	}
	pt_file << std::endl;
	/* We first write down the last power trace*/
	for (int pt = 0; pt < 1; pt ++) {
		for (int i = 0; i < 32; i++) {
			pt_file << vault_power[i] << "\t";
		}
		for (int j = 0; j < 8; j++) {
			for (int i =0; i < 32; i++) {
				pt_file << bank_power[i][j] << "\t";
			}
		}
		for (int i = 0; i < 4; i++) {
			pt_file << power_ialu[i] << "\t" << power_fpalu[i] 
					<< "\t" << power_inssch[i] << "\t" << power_l1i[i]
					<< "\t" << power_insdec[i] << "\t" << power_bp[i]
					<< "\t" << power_ru[i] << "\t" << power_l1d[i]
					<< "\t" << power_mmu[i] << "\t" << power_l2[i] << "\t";
		}
		pt_file << std::endl;

		/* DEBUG: Here Record Power Trace*/
		recordPowerTrace();

	}


	/* Here we update the power data*/
	updatePower();

	/* Here we output the second trace of HotSpot Input*/
	for (int pt = 0; pt < 1; pt ++) {

		
		for (int j = 0; j < 8; j++) {
			for (int i =0; i < 32; i++) {
				pt_file << bank_power[i][j] << "\t";
			}
		}
		for (int i = 0; i < 32; i++) {
			pt_file << vault_power[i] << "\t";
		}
		for (int i = 0; i < 4; i++) {
			pt_file << power_ialu[i] << "\t" << power_fpalu[i] 
					<< "\t" << power_inssch[i] << "\t" << power_l1i[i]
					<< "\t" << power_insdec[i] << "\t" << power_bp[i]
					<< "\t" << power_ru[i] << "\t" << power_l1d[i]
					<< "\t" << power_mmu[i] << "\t" << power_l2[i] << "\t";
		}
		pt_file << std::endl;
	}

	pt_file.close();
}

void
StatsManager::callHotSpot()
{
	//std::cout << "[STAT_DEBUG] Begin Calling HotSpot" << std::endl;
#pragma GCC diagnostic ignored "-Wwrite-strings"
	char *argv[17];
	if (first_ttrace == false) {
		first_ttrace = true;
		/*First run to get steady states*/
		argv[1] = "-c";
		argv[2] = "./HotSpot/hotspot.config";
		argv[3] = "-steady_file";
		argv[4] = "./HotSpot/init.steady";
		argv[5] = "-f";
		argv[6] = "./HotSpot/core_layer.flr";
		argv[7] = "-p";
		argv[8] = "./HotSpot/powertrace.input";
		argv[9] = "-model_type";
		argv[10] = "grid";
		argv[11] = "-grid_layer_file";
		argv[12] = "./HotSpot/test_3D.lcf";
		argv[13] = "-init_file";
		argv[14] = "./HotSpot/init.steady";

		if (reverse_flp) {
			argv[12] = "./HotSpot/reverse_3D.lcf";
		}
		bool use_default_init_temp = Sim()->getCfg()->getBoolDefault("perf_model/thermal/default_init_temp", false);
		hotspot->initHotSpot(15, argv, use_default_init_temp);
		//hotspot->startWarmUp();
		//hotspot->calculateTemperature(unit_temp);
		//hotspot->endHotSpot();

		//hotspot->calculateTemperature(unit_temp, 13, argv);
	} 
	/*
	else {
		int rename_rst;
		char old_name[] = "./HotSpot/last.steady";
		char new_name[] = "./HotSpot/init.steady";
		if (hotspot->do_transient == TRUE || hotspot->start_analysis == TRUE) {
			std::cout << "[Steady File Rename] Rename init file!\n";
			rename_rst = rename(old_name, new_name);
			if (rename_rst == 0) {
				//std::cout << "[Steady File Rename] Success!\n";
			} else {
				std::cout << "[Steady File Rename] Failed!\n";
			}
		}
	}
	*/
	//hotspot->calculateTemperature(unit_temp);


	/*Second run to get final output*/
	argv[1] = "-c";
	argv[2] = "./HotSpot/hotspot.config";
  	argv[3] = "-init_file";
  	argv[4] = "./HotSpot/init.steady";
  	argv[5] = "-f";
  	argv[6] = "./HotSpot/core_layer.flr";
  	argv[7] = "-p";
  	argv[8] = "./HotSpot/powertrace.input";
  	argv[9] = "-o";
  	argv[10] = "./HotSpot/ttrace.output";
  	argv[11] = "-model_type";
  	argv[12] = "grid";
  	argv[13] = "-grid_layer_file";
  	argv[14] = "./HotSpot/test_3D.lcf";
	argv[15] = "-steady_file";
	argv[16] = "./HotSpot/last.steady";



	if (reverse_flp)
		argv[14] = "./HotSpot/reverse_3D.lcf";

	//hotspot->initHotSpot(17, argv);
	hotspot->calculateTemperature(unit_temp);
	//hotspot->endHotSpot();
	//hotspot->calculateTemperature(unit_temp, 17, argv);

	//std::cout << "[STAT_DEBUG] End second Calling HotSpot" << std::endl;
	/*Debug for temperature*/
	double max_temp, max_cntlr_temp, max_bank_temp;
	double avg_temp_cpu, avg_temp_dram;
	vector<double> core_avg_temp(4, 0.0);
	vector<double> core_max_temp(4, 0.0);
	max_temp = max_cntlr_temp = max_bank_temp = avg_temp_cpu = avg_temp_dram = 0;

	if (dump_trace)
		temp_trace_log << "-------trace_" << ttrace_num << "-----"  
					   << "current_interval: " << m_record_interval.getUS() 
					   << std::endl;

	ttrace_num ++;

	for (int i = 0; i < unit_num; i++) {
		if (dump_trace)
			temp_trace_log << "[Sniper] UnitName: " << unit_names[i] 
						   << ", UnitTemp: " << unit_temp[i];
		if (unit_temp[i] > max_temp) max_temp = unit_temp[i];
		if (i < 40) {
			avg_temp_cpu += unit_temp[i];
			int core_id = i / 10;
			core_avg_temp[core_id] += unit_temp[i];
			if (core_max_temp[core_id] < unit_temp[i])
				core_max_temp[core_id] = unit_temp[i];
		}
		if (i >= 40 && i < 72) {
			avg_temp_dram += unit_temp[i];
			if (dump_trace)
				temp_trace_log << ", Cntlr_" << i - 40 
							   << " Power: " << vault_power[i-40] 
							   << ", Total Access: " << vault_access[i-40]; 
			if (unit_temp[i] > max_cntlr_temp) max_cntlr_temp = unit_temp[i];
		}
		if (i >= 72) {
			int v_i = (i - 72) % 32, b_i = (i - 72) / 32;
			BankStatEntry *tmp = &bank_stats_interval[v_i][b_i];
			/* Here we log temperature and access of banks*/
			if (dump_trace)
				temp_trace_log << ", Bank Power: " << bank_power[v_i][b_i] 
					           << ", Total Access: " << tmp->reads + tmp->writes;

			if (unit_temp[i] > max_bank_temp) max_bank_temp = unit_temp[i];

			/* (REMAP_MAN) Here We Want to Update Temperature 
			 * Here we set vault controller temperature
			 * to any bank in the vault (because of temperature sensor)
			 */
			double vault_temp = unit_temp[40 + v_i],
				bank_temp = unit_temp[i];
			m_stacked_dram_unison->updateTemperature(v_i, b_i, bank_temp, vault_temp);
			if (prev_bank_temp[v_i][b_i] > 85) {
				hot_access[v_i][b_i] += tmp->reads + tmp->writes;
				if (prev_bank_temp[v_i][b_i] > 95) {
					err_access[v_i][b_i] += tmp->reads + tmp->writes;
				}
			} else {
				if (m_stacked_dram_unison->enter_roi)
					cool_access[v_i][b_i] += tmp->reads + tmp->writes;
			}
			prev_bank_temp[v_i][b_i] = unit_temp[i];
		}
		if (dump_trace)
			temp_trace_log << std::endl;
	}

	/* Here we choose what to do with DVFS */
	avg_temp_cpu /= 40;
	avg_temp_dram /= 32;

	for (int i = 0; i < 4; i++) {
		core_avg_temp[i] /= 10;
	}
	int temp_t = Sim()->getCfg()->getInt("perf_model/thermal/temperature_type");
	if (temp_t == 0) {
		checkDTM(core_avg_temp, max_bank_temp);
	} else {
		checkDTM(core_max_temp, max_bank_temp);
	}

	/*
	int cache_reads = m_stacked_dram_unison->tot_reads,
		cache_writes = m_stacked_dram_unison->tot_writes,
	    cache_misses = m_stacked_dram_unison->tot_misses;
		*/
	//double miss_rate = 0;
	//double cur_time_ms = double(m_current_time.getFS()) * 1.0e-12;
	//if (cache_misses != 0)
	//	miss_rate = double(cache_misses) / double(cache_reads + cache_writes);

	/*[NEW_EXP] call management*/
	m_stacked_dram_unison->tryRemapping();
	m_stacked_dram_unison->clearCacheStats();
}

void
StatsManager::updateCurrentTime(SubsecondTime t)
{
	if (t > m_current_time) {
		m_current_time = t;
	}
	SubsecondTime time_elasped = m_current_time - m_last_remap_time;
	if (time_elasped > RemapInterval) {
		m_last_remap_time = m_current_time;
		/*
		if (m_stacked_dram_unison->enter_roi && do_remap)
			m_stacked_dram_unison->tryRemapping();
			*/
	}
	
}

double
StatsManager::getDramCntlrTemp(UInt32 vault_num)
{
	return unit_temp[40 + vault_num];
}

double
StatsManager::getDramBankTemp(UInt32 vault_num, UInt32 bank_num)
{
	return unit_temp[40 + 32 + bank_num * 32 + vault_num];
}

int
timeDuration(std::chrono::steady_clock::time_point t1, std::chrono::steady_clock::time_point t2) {
	auto duration =  std::chrono::duration_cast<std::chrono::microseconds> (t1 - t2);
	return duration.count();
}

void
StatsManager::recordStats(String prefix)
{
	std::cout << "\n************recordStats once at " << m_current_time.getUS() << std::endl;

	auto start = std::chrono::steady_clock::now();
	//std::cout << "[TIME_REC]Current Time in Chrono: " << timeDuration(start, last_time_point) << std::endl;

	m_record_interval = m_current_time - m_last_record_time;
	if (m_record_interval == SubsecondTime::Zero()) {
		m_record_interval = SubsecondTime::MS(1);
	}
	m_last_record_time = m_current_time;

   LOG_ASSERT_ERROR(m_db, "m_db not yet set up !?");

   // Allow lazily-maintained statistics to be updated
   Sim()->getHooksManager()->callHooks(HookType::HOOK_PRE_STAT_WRITE, (UInt64)prefix.c_str());

   int res;
   int prefixid = ++m_prefixnum;

   res = sqlite3_exec(m_db, "BEGIN TRANSACTION", NULL, NULL, NULL);
   LOG_ASSERT_ERROR(res == SQLITE_OK, "Error executing SQL statement: %s", sqlite3_errmsg(m_db));

   sqlite3_reset(m_stmt_insert_prefix);
   sqlite3_bind_int(m_stmt_insert_prefix, 1, prefixid);
   sqlite3_bind_text(m_stmt_insert_prefix, 2, prefix.c_str(), -1, SQLITE_TRANSIENT);
   res = sqlite3_step(m_stmt_insert_prefix);
   LOG_ASSERT_ERROR(res == SQLITE_DONE, "Error executing SQL statement: %s", sqlite3_errmsg(m_db));
   /* Update DRAM statistics*/
   m_stacked_dram_unison->updateStats();

   auto end = std::chrono::steady_clock::now();

   //std::cout << "[TIME_REC]Time spent before dumpHotspotInput() is: " << timeDuration(end, start) << std::endl;
   start = end;

   /*prepare power data for HotSpot*/
   if (reverse_flp == false)
      dumpHotspotInput();
   else
      dumpHotspotInputReverse();

   end = std::chrono::steady_clock::now();
   //std::cout << "[TIME_REC]Time spent on dumpHotspotInput() is: " << timeDuration(end, start) << std::endl;
   start = end;

   /* Call HotSpot in Sniper*/
   if (dyn_power_proc > 0.001 || start_hotspot) {
      callHotSpot();
	  if (start_hotspot == false) {
		  start_hotspot = true;
	  }
   } else {
	   std::cout << "[Warning] the power of processor is too small, we skip the temperature calculation!\n";
   }
   end = std::chrono::steady_clock::now();
   //std::cout << "[TIME_REC]Time spent on callHotSpot() is: " << timeDuration(end, start) << std::endl;
   start = end;

   /* (REMAP_MAN) decide whether to remap*/
   //m_stacked_dram_unison->tryRemapping();

   /* Dump power trace during runtime*/
   //dumpDramPowerTrace();

   for(StatsObjectList::iterator it1 = m_objects.begin(); it1 != m_objects.end(); ++it1)
   {
      for (StatsMetricList::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
      {
         for(StatsIndexList::iterator it3 = it2->second.second.begin(); it3 != it2->second.second.end(); ++it3)
         {
            if (!it3->second->isDefault())
            {
               sqlite3_reset(m_stmt_insert_value);
               sqlite3_bind_int(m_stmt_insert_value, 1, prefixid);
               sqlite3_bind_int(m_stmt_insert_value, 2, it2->second.first);   // Metric ID
               sqlite3_bind_int(m_stmt_insert_value, 3, it3->second->index);  // Core ID
               sqlite3_bind_int64(m_stmt_insert_value, 4, it3->second->recordMetric());
               res = sqlite3_step(m_stmt_insert_value);
               LOG_ASSERT_ERROR(res == SQLITE_DONE, "Error executing SQL statement: %s", sqlite3_errmsg(m_db));
            }
         }
      }
   }
   res = sqlite3_exec(m_db, "END TRANSACTION", NULL, NULL, NULL);
   LOG_ASSERT_ERROR(res == SQLITE_OK, "Error executing SQL statement: %s", sqlite3_errmsg(m_db));

   end = std::chrono::steady_clock::now();
   //std::cout << "[TIME_REC]Time spent after callHotSpot() is: " << timeDuration(end, start) << std::endl;
   last_time_point = end;
}

void
StatsManager::registerMetric(StatsMetricBase *metric)
{
   std::string _objectName(metric->objectName.c_str()), _metricName(metric->metricName.c_str());

   /*print registerred metric*/
   //printf("StatsManager::registerMetric: ObjName: %s, MtrName: %s\n", _objectName.c_str(), _metricName.c_str());

   LOG_ASSERT_ERROR(m_objects[_objectName][_metricName].second.count(metric->index) == 0,
      "Duplicate statistic %s.%s[%d]", _objectName.c_str(), _metricName.c_str(), metric->index);
   m_objects[_objectName][_metricName].second[metric->index] = metric;

   if (m_objects[_objectName][_metricName].first == 0)
   {
      m_objects[_objectName][_metricName].first = ++m_keyid;
      if (m_db)
      {
         // Metrics name record was already written, but a new metric was registered afterwards: write a new record
         recordMetricName(m_keyid, _objectName, _metricName);
      }
   }
}

StatsMetricBase *
StatsManager::getMetricObject(String objectName, UInt32 index, String metricName)
{
   std::string _objectName(objectName.c_str()), _metricName(metricName.c_str());
   if (m_objects.count(_objectName) == 0)
      return NULL;
   if (m_objects[_objectName].count(_metricName) == 0)
      return NULL;
   if (m_objects[_objectName][_metricName].second.count(index) == 0)
      return NULL;
   return m_objects[_objectName][_metricName].second[index];
}

void
StatsManager::logTopology(String component, core_id_t core_id, core_id_t master_id)
{
   sqlite3_stmt *stmt;
   sqlite3_prepare(m_db, "INSERT INTO topology (componentname, coreid, masterid) VALUES (?, ?, ?);", -1, &stmt, NULL);
   sqlite3_bind_text(stmt, 1, component.c_str(), -1, SQLITE_TRANSIENT);
   sqlite3_bind_int(stmt, 2, core_id);
   sqlite3_bind_int(stmt, 3, master_id);
   int res = sqlite3_step(stmt);
   LOG_ASSERT_ERROR(res == SQLITE_DONE, "Error executing SQL statement: %s", sqlite3_errmsg(m_db));
   sqlite3_finalize(stmt);
}

void
StatsManager::logEvent(event_type_t event, SubsecondTime time, core_id_t core_id, thread_id_t thread_id, UInt64 value0, UInt64 value1, const char * description)
{
   if (time == SubsecondTime::MaxTime())
      time = Sim()->getClockSkewMinimizationServer()->getGlobalTime();

   sqlite3_stmt *stmt;
   sqlite3_prepare(m_db, "INSERT INTO event (event, time, core, thread, value0, value1, description) VALUES (?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL);
   sqlite3_bind_int(stmt, 1, event);
   sqlite3_bind_int64(stmt, 2, time.getFS());
   sqlite3_bind_int(stmt, 3, core_id);
   sqlite3_bind_int(stmt, 4, thread_id);
   sqlite3_bind_int64(stmt, 5, value0);
   sqlite3_bind_int64(stmt, 6, value1);
   sqlite3_bind_text(stmt, 7, description ? description : "", -1, SQLITE_STATIC);
   int res = sqlite3_step(stmt);
   LOG_ASSERT_ERROR(res == SQLITE_DONE, "Error executing SQL statement: %s", sqlite3_errmsg(m_db));
   sqlite3_finalize(stmt);
}

StatHist &
StatHist::operator += (StatHist & stat)
{
   if (n == 0) { min = stat.min; max = stat.max; }
   n += stat.n;
   s += stat.s;
   s2 += stat.s2;
   if (stat.n && stat.min < min) min = stat.min;
   if (stat.n && stat.max > max) max = stat.max;
   for(int i = 0; i < HIST_MAX; ++i)
      hist[i] += stat.hist[i];
   return *this;
}

void
StatHist::update(unsigned long v)
{
   if (n == 0) {
      min = v;
      max = v;
   }
   n++;
   s += v;
   s2 += v*v;
   if (v < min) min = v;
   if (v > max) max = v;
   int bin = floorLog2(v) + 1;
   if (bin >= HIST_MAX) bin = HIST_MAX - 1;
      hist[bin]++;
}

void
StatHist::print()
{
   printf("n(%lu), avg(%.2f), std(%.2f), min(%lu), max(%lu), hist(%lu",
      n, n ? s/float(n) : 0, n ? sqrt((s2/n - (s/n)*(s/n))*n/float(n-1)) : 0, min, max, hist[0]);
   for(int i = 1; i < HIST_MAX; ++i)
      printf(",%lu", hist[i]);
   printf(")\n");
}
