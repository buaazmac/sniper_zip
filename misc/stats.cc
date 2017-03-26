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
		   std::cout << "/*BANK*/" << i << "->" << j
					 << ": cool access (" << cool_access[i][j] << "), "
					 << "hot access (" << hot_access[i][j] << ")." << std::endl;
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
   temp_trace_log.open(ttrace_file);
   ttrace_num = 0;
   /*Initial Hotspot*/
   hotspot = new Hotspot();
   unit_names = alloc_names(MAX_UNITS, STR_SIZE);
   unit_temp = dvector(MAX_UNITS);
   /*Get Unit names*/
   unit_num = 0;
   hotspot->getNames("./HotSpot/powertrace.input", unit_names, &unit_num);
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
	   }
   }
   /*Initialize current time*/
   m_current_time = SubsecondTime::Zero();
   m_last_remap_time = SubsecondTime::Zero();
   m_last_record_time = SubsecondTime::Zero();
   m_record_interval = SubsecondTime::Zero();

   /* Initialize remap interval*/
   RemapInterval = SubsecondTime::US(50);

   first_ttrace = false;

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

double 
StatsManager::computeDramPower(SubsecondTime tACT, SubsecondTime tPRE, SubsecondTime tRD, SubsecondTime tWR, SubsecondTime totT, UInt32 reads, UInt32 writes, double page_hit_rate)
{
	double act_t = double(tACT.getFS()), pre_t = double(tPRE.getFS()), read_t = double(tRD.getFS()), write_t = double(tWR.getFS()), tot_time = double(totT.getFS());
	if (act_t > tot_time) {
		std::cout << "[Potetial Error] tACT > tTOT\n";
		act_t = tot_time;
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
    int rd_per = 50;
	if (reads + writes != 0)
		rd_per = (100 * reads) / (reads + write_t);

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
StatsManager::computeDramCntlrPower(UInt32 reads, UInt32 writes, SubsecondTime t)
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
	return power_socket_dyn * sockets;
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
	   std::cout << "Stacked dram cache has " << m_stacked_dram_unison->n_vaults << " vaults!" << std::endl;

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

		dram_stats_file << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
		dram_stats_file << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
		dram_stats_file << "@" << std::endl;
   }

   /*
	Alloy Cache Stats
	  */
   if (m_stacked_dram_alloy != NULL) {
	   std::cout << "Stacked dram cache has " << m_stacked_dram_alloy->n_vaults << " vaults!" << std::endl;

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

		dram_stats_file << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
		dram_stats_file << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
		dram_stats_file << "@" << std::endl;
   }

   /*
	PoM Stats
	  */
   if (m_stacked_dram_mem != NULL) {
	   std::cout << "Stacked dram mem has " << m_stacked_dram_mem->n_vaults << " vaults!" << std::endl;
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

		dram_stats_file << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
		dram_stats_file << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
		dram_stats_file << "@" << std::endl;
   }
#endif
}

void
StatsManager::updateBankStat(int i, int j, BankPerfModel* bank)
{
	bank_stats[i][j] = bank->stats;
}

void
StatsManager::dumpHotspotInput()
{
	//std::cout << "[STAT_DEBUG] Begin Dumping HotspotInput\n" << std::endl;
	/* Here we dump input for hotspot
	 * init_file(tmp.steady) from last time
	 * -p powertrace.input
	 */

/* Write down power trace*/
	std::ofstream pt_file;
	pt_file.open("./HotSpot/powertrace.input");
	pt_file << "L3";
	for (int i = 0; i < 4; i++) {
		pt_file << "\texe_" << i;
		pt_file << "\tifetch_" << i;
		pt_file << "\tlsu_" << i;
		pt_file << "\tmmu_" << i;
		pt_file << "\tl2_" << i;
		pt_file << "\tru_" << i;
	}
	for (int i = 0; i < 32; i++) {
		pt_file << "\tdram_ctlr_" << i;
	}
	for (int j = 0; j < 8; j++) {
		for (int i = 0; i < 32; i++) {
			pt_file << "\tdram_" << i << "_" << j;
		}
	}
	pt_file << std::endl;
	/* We first write down the last power trace*/
	for (int pt = 0; pt < 1; pt ++) {
		pt_file << power_L3;

		for (int i = 0; i < 4; i++) {
			pt_file << "\t" << power_exe[i] << "\t" << power_ifetch[i] 
					<< "\t" << power_lsu[i] << "\t" << power_mmu[i]
					<< "\t" << power_l2[i] << "\t" << power_ru[i];
		}
		for (int i = 0; i < 32; i++) {
			pt_file << "\t" << vault_power[i];
		}
		for (int j = 0; j < 8; j++) {
			for (int i =0; i < 32; i++) {
				pt_file << "\t" << bank_power[i][j];
			}
		}
		pt_file << std::endl;

		/* Here we output power trace for drawing picture*/
		power_trace_log << power_L3;
		for (int i = 0; i < 4; i++) {
			power_trace_log << "\t" << power_exe[i] << "\t" << power_ifetch[i] 
					<< "\t" << power_lsu[i] << "\t" << power_mmu[i]
					<< "\t" << power_l2[i] << "\t" << power_ru[i];
		}
		for (int i = 0; i < 32; i++) {
			power_trace_log << "\t" << vault_power[i];
		}
		for (int j = 0; j < 8; j++) {
			for (int i =0; i < 32; i++) {
				power_trace_log << "\t" << bank_power[i][j];
			}
		}
		power_trace_log << std::endl;
		/**/
	}
//for (int pt_it = 0; pt_it < 2; pt_it++) {
	/*
		Here we update power statistics!
	 */
	power_L3 = double(getMetricObject("L3", 0, "power-dynamic")->recordMetric()) * 1.0e-6;
	//pt_file << power_L3;
	for (int i = 0; i < 4; i++) {
		power_exe[i] = double(getMetricObject("exe", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_ifetch[i] = double(getMetricObject("ifetch", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_lsu[i] = double(getMetricObject("lsu", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_mmu[i] = double(getMetricObject("mmu", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_l2[i] = double(getMetricObject("L2", i, "power-dynamic")->recordMetric()) * 1.0e-6;
		power_ru[i] = double(getMetricObject("ru", i, "power-dynamic")->recordMetric()) * 1.0e-6;
	}

	/* Set of time interval*/
	SubsecondTime tot_time = m_record_interval;

	UInt32 n_banks = 8;
	UInt32 n_vaults = m_stacked_dram_unison->n_vaults;

	//std::ofstream test_file;
	//test_file.open("./runtime_data.txt", std::ofstream::out | std::ofstream::app);

   if (m_stacked_dram_unison != NULL) {

	   //test_file << "-----------OUTPUT_DATA-----------------" << std::endl;

		for (UInt32 i = 0; i < m_stacked_dram_unison->n_vaults; i++) {
			double std_power = 0;
			VaultPerfModel* vault = m_stacked_dram_unison->m_vaults_array[i];
			n_banks = vault->n_banks;

			UInt32 tot_reads, tot_writes, tot_row_hits;
			tot_reads = vault->stats.reads - vault_reads[i];
			tot_writes = vault->stats.writes - vault_writes[i];
			tot_row_hits = vault->stats.row_hits - vault_row_hits[i];
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

				bank_stats_interval[i][j].tRD = SubsecondTime::Zero();
				bank_stats_interval[i][j].tWR = SubsecondTime::Zero();

				//printf("#####init: %d, interval: %d, total: %d\n", bank->stats.reads, bank_stats_interval[i][j].reads, bank_stats[i][j].reads);

				/*Update current bank statistics*/
				updateBankStat(i, j, bank);

				BankStatEntry *tmp = &bank_stats_interval[i][j];

				double page_hit_rate = 0.5;
				if (tot_reads + tot_writes != 0) {
					page_hit_rate = double(tot_row_hits) / double(tot_reads + tot_writes);
				}
				bank_power[i][j] = computeDramPower(tmp->tACT, tmp->tPRE, tmp->tRD, tmp->tWR, tot_time, tmp->reads, tmp->writes, page_hit_rate);
				std_power += bank_power[i][j];


				/*
				test_file << "bank_" << i << "_" << j
						  << ": power: " << bank_power[i][j]
						  << ", reads: " << tmp->reads
						  << ", writes: " << tmp->writes
						  << ", t_act: " << tmp->tACT.getUS()
						  << ", t_pre: " << tmp->tPRE.getUS()
						  << ", t_rd: " << tmp->tRD.getUS()
						  << ", t_wr: " << tmp->tWR.getUS()
						  << ", t_tot: " << tot_time.getUS()
						  << ", page_hit: " << page_hit_rate
						  <<std::endl;
				*/
				//tot_reads += tmp->reads;
				//tot_writes += tmp->writes;
			}
			vault_power[i] = std_power + computeDramCntlrPower(tot_reads, tot_writes, tot_time);
			vault_access[i] = tot_reads + tot_writes;

			/*
			test_file << "---------------------------" << std::endl;
			test_file << "vault_" << i 
				<< ": power: " << vault_power[i]
				<< ", reads: " << tot_reads
				<< ", writes: " << tot_writes << std::endl;
			*/
		}
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


	for (int pt = 0; pt < 1; pt ++) {
		pt_file << power_L3;

		for (int i = 0; i < 4; i++) {
			pt_file << "\t" << power_exe[i] << "\t" << power_ifetch[i] 
					<< "\t" << power_lsu[i] << "\t" << power_mmu[i]
					<< "\t" << power_l2[i] << "\t" << power_ru[i];
		}
		for (int i = 0; i < 32; i++) {
			pt_file << "\t" << vault_power[i];
		}
		for (int j = 0; j < 8; j++) {
			for (int i =0; i < 32; i++) {
				pt_file << "\t" << bank_power[i][j];
			}
		}
		pt_file << std::endl;
	}

	pt_file.close();
	//test_file.close();
	//std::cout << "[STAT_DEBUG] End Dumping HotspotInput" << std::endl;
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
		argv[4] = "./HotSpot/tmp.steady";
		argv[5] = "-f";
		argv[6] = "./HotSpot/core_layer.flr";
		argv[7] = "-p";
		argv[8] = "./HotSpot/powertrace.input";
		argv[9] = "-model_type";
		argv[10] = "grid";
		argv[11] = "-grid_layer_file";
		argv[12] = "./HotSpot/test_3D.lcf";

		hotspot->calculateTemperature(unit_temp, 13, argv);
	} else {
		int rename_rst;
		char old_name[] = "./HotSpot/tmp.steady.new";
		char new_name[] = "./HotSpot/tmp.steady";
		rename_rst = rename(old_name, new_name);
		if (rename_rst == 0) {
			//std::cout << "[Steady File Rename] Success!\n";
		} else {
			//std::cout << "[Steady File Rename] Failed!\n";
		}
	}

	/*Second run to get final output*/
	argv[1] = "-c";
	argv[2] = "./HotSpot/hotspot.config";
  	argv[3] = "-init_file";
  	argv[4] = "./HotSpot/tmp.steady";
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
	argv[16] = "./HotSpot/tmp.steady.new";

  	hotspot->calculateTemperature(unit_temp, 17, argv);

	/*Debug for temperature*/
	double max_temp, max_cntlr_temp, max_bank_temp;
	max_temp = max_cntlr_temp = max_bank_temp = 0;
	char *s1, *s2, *s3;

	temp_trace_log << "-------trace_" << ttrace_num << "-----" << std::endl;
	ttrace_num ++;

	for (int i = 0; i < unit_num; i++) {
		if (i < 25) {
			if (max_temp < unit_temp[i]) {
				max_temp = unit_temp[i];
				s1 = unit_names[i];
			}
		} else if (i < 57) {
			if (max_cntlr_temp < unit_temp[i]) {
				max_cntlr_temp = unit_temp[i];
				s2 = unit_names[i];
			}
		} else {
			if (max_bank_temp < unit_temp[i]) {
				max_bank_temp = unit_temp[i];
				s3 = unit_names[i];
			}
		}
		temp_trace_log << "[Sniper] UnitName: " << unit_names[i] << ", UnitTemp: " << unit_temp[i];
		if (i >= 25 && i < 57) {
			temp_trace_log << ", Cntlr_" << i - 25 
				<< " Power: " << vault_power[i-25] << ", Total Access: " << vault_access[i-25]; 
		}
		if (i >= 57) {
			int v_i = (i - 57) % 32, b_i = (i - 57) / 32;
			BankStatEntry *tmp = &bank_stats_interval[v_i][b_i];
			temp_trace_log << ", Bank_" << v_i << "_" << b_i 
				<< " Power: " << bank_power[v_i][b_i] << ", Total Access: " << tmp->reads + tmp->writes; 
			/* (REMAP_MAN) Here We Want to Update Temperature 
			 * Here we set vault controller temperature
			 * to any bank in the vault (because of temperature sensor)
			 */
			int vault_temp = int(unit_temp[25 + v_i]),
				bank_temp = int(unit_temp[i]);
			m_stacked_dram_unison->updateTemperature(v_i, b_i, bank_temp, vault_temp);
			if (unit_temp[i] > 85) {
				hot_access[v_i][b_i] += tmp->reads + tmp->writes;
			} else {
				cool_access[v_i][b_i] += tmp->reads + tmp->writes;
			}
		}
		temp_trace_log << std::endl;
	}
	int cache_reads = m_stacked_dram_unison->tot_reads,
		cache_writes = m_stacked_dram_unison->tot_writes,
	    cache_misses = m_stacked_dram_unison->tot_misses;
	double miss_rate = 0;
	double cur_time_ms = double(m_current_time.getFS()) * 1.0e-12;
	if (cache_misses != 0)
		miss_rate = double(cache_misses) / double(cache_reads + cache_writes);
	temp_trace_log << "TotAccess: " << cache_reads + cache_writes 
				   << " MissRate: " << miss_rate  
				   << " CurrentTIme: " << cur_time_ms << std::endl;
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
		if (m_stacked_dram_unison->enter_roi)
			m_stacked_dram_unison->tryRemapping();
	}
	
}

double
StatsManager::getDramCntlrTemp(UInt32 vault_num)
{
	return unit_temp[25 + vault_num];
}

double
StatsManager::getDramBankTemp(UInt32 vault_num, UInt32 bank_num)
{
	return unit_temp[25 + 32 + bank_num * 32 + vault_num];
}

void
StatsManager::recordStats(String prefix)
{
	std::cout << "recordStats once at " << m_current_time.getUS() << std::endl;

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

   /*prepare power data for HotSpot*/
   dumpHotspotInput();

   /* Call HotSpot in Sniper*/
   callHotSpot();

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
