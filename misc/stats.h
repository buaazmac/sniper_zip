#pragma once

#include "simulator.h"
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
	  const SubsecondTime RemapInterval = SubsecondTime::US(1);
	  SubsecondTime m_current_time;
	  SubsecondTime m_last_remap_time;
	  SubsecondTime m_last_record_time;
	  SubsecondTime m_record_interval;

	  bool first_ttrace;

	  struct BankStatEntry bank_stats[32][8];
	  UInt64 vault_reads[32], vault_writes[32], vault_row_hits[32];
	  BankStatEntry bank_stats_interval[32][8];
	  UInt32 vault_access[32];
	  /* Write power of dram*/
	  double bank_power[32][8];
	  double vault_power[32];
	  double power_L3;
	  double power_exe[4], power_ifetch[4], power_lsu[4], power_mmu[4], power_l2[4], power_ru[4];
	  int hot_access[32][8], cool_access[32][8];
	  
	  /*Temperature data*/
	  int unit_num;
	  char **unit_names;
	  double *unit_temp;

	  const char *ttrace_file = "./test_ttrace.txt";
	  int ttrace_num;
	  std::ofstream power_trace_log;
	  std::ofstream temp_trace_log;
	  void dumpDramPowerTrace();
	  void updateBankStat(int i, int j, BankPerfModel* bank);
	  /*Hotspot*/
	  void dumpHotspotInput();
	  void callHotSpot();
	  Hotspot *hotspot;
	  /*Calculate DRAM power*/
	  struct DramTable {
		  double maxVcc; //1.26V
		  double Vdd;	//1.2V
		  double Idd2P;  //34mA
		  double Idd2N;  //50mA
		  double Idd3P;  //40mA
		  double Idd3N;  //65mA
		  double Idd5;   //175mA
		  double Idd4W;  //195mA
		  double Idd0;   //65mA
		  double RFC_min;//260ns
		  double REFI;   //7800ns
		  double tRAS;   //32ns
		  double tRC;    //45.5ns
		  double tCKavg; //0.75ns
		  int burstLen; //8
	  } dram_table = {1.26, 1.2, 34, 50, 40, 65, 175, 195, 65, 260, 7800, 32, 45.5, 0.75, 8};
	  struct DramCntlrTable {
		  double DRAM_CLK;
		  double DRAM_POWER_READ;
		  double DRAM_POWER_WRITE;
		  double num_dram_controllers;
		  double chips_per_dimm;
		  double dimms_per_socket;
	  } dram_cntlr_table = {800.0, 0.678, 0.825, 1.0, 8.0, 4.0};

	  double computeDramPower(SubsecondTime tACT, SubsecondTime tPRE, SubsecondTime tRD, SubsecondTime tWR, SubsecondTime totT, UInt32 reads, UInt32 writes, double page_hit_rate);
	  double computeDramCntlrPower(UInt32 reads, UInt32 writes, SubsecondTime t);


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
