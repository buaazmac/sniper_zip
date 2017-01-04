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
         recordMetricName(it2->second.first, it1->first, it2->first);
      }
   }
   sqlite3_exec(m_db, "END TRANSACTION", NULL, NULL, NULL);
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
StatsManager::recordStats(String prefix)
{
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

// output stacked dram stats
   /*
	Unison Cache Stats
	  */
   if (m_stacked_dram_unison != NULL) {
	   std::cout << "Stacked dram cache has " << m_stacked_dram_unison->n_vaults << " vaults!" << std::endl;

		SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
		tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

		UInt32 tot_access, tot_row_hits;

		tot_access = tot_row_hits = 0;

	    dram_stats_file << "@" <<  prefix << std::endl;
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
