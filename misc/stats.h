#pragma once

#include "time.h"
#include <chrono>

#include "simulator.h"
#include "dvfs_manager.h"
#include "itostr.h"

#include "../performance_model/stacked_dram_cntlr.h"
#include "../performance_model/dram_vault.h"
#include "../performance_model/dram_bank.h"

#include "./HotSpot/hotspot.h"
#include "./HotSpot/util.h"

#include <strings.h>
#include <sqlite3.h>

#include <fstream>

class StatsMetricBase
{
   public:
      String objectName;
      UInt32 index;
      String metricName;
      StatsMetricBase(String _objectName, UInt32 _index, String _metricName) :
         objectName(_objectName), index(_index), metricName(_metricName)
      {}
      virtual ~StatsMetricBase() {}
      virtual UInt64 recordMetric() = 0;
      virtual bool isDefault() { return false; } // Return true when value hasn't changed from its initialization value
};

template <class T> UInt64 makeStatsValue(T t);

template <class T> class StatsMetric : public StatsMetricBase
{
   public:
      T *metric;
      StatsMetric(String _objectName, UInt32 _index, String _metricName, T *_metric) :
         StatsMetricBase(_objectName, _index, _metricName), metric(_metric)
      {}
      virtual UInt64 recordMetric()
      {
         return makeStatsValue<T>(*metric);
      }
      virtual bool isDefault()
      {
         return recordMetric() == 0;
      }
};

typedef UInt64 (*StatsCallback)(String objectName, UInt32 index, String metricName, UInt64 arg);
class StatsMetricCallback : public StatsMetricBase
{
   public:
      StatsCallback func;
      UInt64 arg;
      StatsMetricCallback(String _objectName, UInt32 _index, String _metricName, StatsCallback _func, UInt64 _arg) :
         StatsMetricBase(_objectName, _index, _metricName), func(_func), arg(_arg)
      {}
      virtual UInt64 recordMetric()
      {
         return func(objectName, index, metricName, arg);
      }
};

class StackedDramPerfUnison;
class StackedDramPerfAlloy;
class StackedDramPerfMem;
class VaultPerfModel;
class BankPerfModel;

class StatsManager
{
   public:
      // Event type                 core              thread            arg0           arg1              description
      typedef enum {
         EVENT_MARKER = 1,       // calling core      calling thread    magic arg0     magic arg1        str (SimMarker/SimNamedMarker)
         EVENT_THREAD_NAME,      // calling core      calling thread    0              0                 thread name (SimSetThreadName)
         EVENT_APP_START,        // -1                -1                app id         0                 ""
         EVENT_APP_EXIT,         // -1                -1                app id         0                 ""
         EVENT_THREAD_CREATE,    // initial core      created thread    app id         creator thread    ""
         EVENT_THREAD_EXIT,      // current core      exiting thread    0              0                 ""
      } event_type_t;

      StatsManager();
      ~StatsManager();

	  void init_stacked_dram_unison(StackedDramPerfUnison *stacked_dram);
	  void init_stacked_dram_alloy(StackedDramPerfAlloy *stacked_dram);
	  void init_stacked_dram_mem(StackedDramPerfMem *stacked_dram);

      void init();
      void recordStats(String prefix);

	  /* Frequency Vector*/
	  std::vector<UInt64> freq_table; // MHz
	  int freq_lev, max_lev;

	  /* get temperature of DRAM components*/
	  void updateCurrentTime(SubsecondTime t);
	  double getDramCntlrTemp(UInt32 vault_num);
	  double getDramBankTemp(UInt32 vault_num, UInt32 bank_num);

      void registerMetric(StatsMetricBase *metric);
      StatsMetricBase *getMetricObject(String objectName, UInt32 index, String metricName);
      void logTopology(String component, core_id_t core_id, core_id_t master_id);
      void logMarker(SubsecondTime time, core_id_t core_id, thread_id_t thread_id, UInt64 value0, UInt64 value1, const char * description)
      { logEvent(EVENT_MARKER, time, core_id, thread_id, value0, value1, description); }
      void logEvent(event_type_t event, SubsecondTime time, core_id_t core_id, thread_id_t thread_id, UInt64 value0, UInt64 value1, const char * description);

   private:
      UInt64 m_keyid;
      UInt64 m_prefixnum;

      sqlite3 *m_db;
      sqlite3_stmt *m_stmt_insert_name;
      sqlite3_stmt *m_stmt_insert_prefix;
      sqlite3_stmt *m_stmt_insert_value;

	  /*Stacked Dram Controller*/
	  StackedDramPerfUnison *m_stacked_dram_unison;
	  StackedDramPerfAlloy *m_stacked_dram_alloy;
	  StackedDramPerfMem *m_stacked_dram_mem;
	  std::ofstream dram_stats_file;
	  /*Record the previous statistics*/
	  SubsecondTime RemapInterval;
	  bool do_remap;
	  bool reverse_flp;

	  SubsecondTime m_current_time;
	  SubsecondTime m_last_remap_time;
	  SubsecondTime m_last_record_time;
	  SubsecondTime m_record_interval;

	  std::chrono::steady_clock::time_point last_time_point;

	  bool first_ttrace;
	  bool start_hotspot;

	  struct BankStatEntry bank_stats[32][8];
	  UInt64 vault_reads[32], vault_writes[32], vault_row_hits[32];
	  BankStatEntry bank_stats_interval[32][8];
	  UInt32 vault_access[32];
	  /* Write power of dram*/
	  double bank_power[32][8];
	  double prev_bank_temp[32][8];
	  double vault_power[32];
	  double peak_power_proc, dyn_power_proc, power_L3, power_mc;
	  double power_exe[4], power_ifetch[4], power_lsu[4], power_mmu[4], power_l2[4], power_ru[4], power_ialu[4], power_fpalu[4], power_inssch[4], power_l1i[4], power_insdec[4], power_bp[4], power_l1d[4];
	  // Record the previous power value for each component
	  double p_power_L3, p_power_mc;
	  double p_power_exe[4], p_power_ifetch[4], p_power_lsu[4], p_power_mmu[4], p_power_l2[4], p_power_ru[4], p_power_ialu[4], p_power_fpalu[4], p_power_inssch[4], p_power_l1i[4], p_power_insdec[4], p_power_bp[4], p_power_l1d[4];

	  int hot_access[32][8], cool_access[32][8], err_access[32][8];
	  
	  /*Temperature data*/
	  int unit_num;
	  char **unit_names;
	  double *unit_temp;


	  const char *ttrace_file = "./test_ttrace.txt";
	  int ttrace_num;
	  std::ofstream power_trace_log;
	  std::ofstream temp_trace_log;
	  bool dump_trace;
	  void dumpDramPowerTrace();
	  void updateBankStat(int i, int j, BankPerfModel* bank);
	  /*Hotspot*/
	  void recordPowerTrace();
	  void updatePower();
	  void dumpHotspotInput();
	  void dumpHotspotInputReverse();
	  void callHotSpot();
	  Hotspot *hotspot;
	  /*Calculate DRAM power*/
	  struct DramTable {
		  double freq; //500 MHz
		  double maxVcc; //1.575V
		  double Vdd;	//1.5V
		  double Idd2P;  //20mA
		  double Idd2N;  //40mA
		  double Idd3P;  //45mA
		  double Idd3N;  //42mA
		  double Idd5;   //200mA
		  double Idd4W;  //160mA
		  double Idd4R;  //200mA
		  double Idd0;   //70mA
		  double RFC_min;//340ns
		  double REFI;   //7800ns
		  double tRAS;   //37.5ns
		  double tRC;    //52.5ns
		  double tCKavg; //2.5ns
		  double burstLen; //8
	  } dram_table = {500, 1.575, 1.5, 20, 40, 45, 42, 200, 160, 200, 70, 340, 7800, 37.5, 52.5, 2.5, 8};
	  struct DramCntlrTable {
		  double DRAM_CLK;
		  double DRAM_POWER_READ;
		  double DRAM_POWER_WRITE;
		  double num_dram_controllers;
		  double chips_per_dimm;
		  double dimms_per_socket;
	  } dram_cntlr_table = {500.0, 0.678, 0.825, 1.0, 8.0, 4.0};

	  void checkDTM(const vector<double> cpu_temp, double dram_temp); 

	  void setGlobalFrequency(UInt64 freq_in_mhz);
	  void setFrequency(UInt64 core_num, UInt64 freq_in_mhz);

	  double computeBankPower(double bnk_pre, double cke_lo_pre, double page_hit, double WRsch, double RDsch, bool hot, double Vdd_use);

	  double computeDramPower(SubsecondTime tACT, SubsecondTime tPRE, SubsecondTime tRD, SubsecondTime tWR, SubsecondTime totT, UInt32 reads, UInt32 writes, double page_hit_rate);
	  double computeDramCntlrPower(UInt32 reads, UInt32 writes, SubsecondTime t, UInt32 tot_access);


      // Use std::string here because String (__versa_string) does not provide a hash function for STL containers with gcc < 4.6
      typedef std::unordered_map<UInt64, StatsMetricBase *> StatsIndexList;
      typedef std::pair<UInt64, StatsIndexList> StatsMetricWithKey;
      typedef std::unordered_map<std::string, StatsMetricWithKey> StatsMetricList;
      typedef std::unordered_map<std::string, StatsMetricList> StatsObjectList;
      StatsObjectList m_objects;

      static int __busy_handler(void* self, int count) { return ((StatsManager*)self)->busy_handler(count); }
      int busy_handler(int count);

      void recordMetricName(UInt64 keyId, std::string objectName, std::string metricName);
};

template <class T> void registerStatsMetric(String objectName, UInt32 index, String metricName, T *metric)
{
   Sim()->getStatsManager()->registerMetric(new StatsMetric<T>(objectName, index, metricName, metric));
}


class StatHist {
  private:
    static const int HIST_MAX = 20;
    unsigned long n, s, s2, min, max;
    unsigned long hist[HIST_MAX];
    char dummy[64];
  public:
    StatHist() : n(0), s(0), s2(0), min(0), max(0) { bzero(hist, sizeof(hist)); }
    StatHist & operator += (StatHist & stat);
    void update(unsigned long v);
    void print();
};
