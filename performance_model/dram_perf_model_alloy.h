#ifndef __DRAM_PERF_MODEL_ALLOY_H__
#define __DRAM_PERF_MODEL_ALLOY_H__

#include "stacked_dram_cntlr.h"

#include "utils.h"

#include "dram_perf_model.h"
#include "queue_model.h"
#include "fixed_types.h"
#include "subsecond_time.h"
#include "dram_cntlr_interface.h"
#include "distribution.h"

#include "cache_state.h"
#include "cache_base.h"

#include "core.h"
#include "simulator.h"
#include "config.h"

#include <iostream>
#include <fstream>

// TAG size = 72B, DRAM ROW size = 2KB
// each row has 2KB/72B = 28 TAGs
class DramCacheBlockInfo
{
private:
	IntPtr m_tag;
	UInt32 m_vbit;
	UInt32 m_dbit;

	CacheState::cstate_t m_cstate;

public:
	DramCacheBlockInfo();
	DramCacheBlockInfo(IntPtr tag, CacheState::cstate_t cstate);
	~DramCacheBlockInfo();
	static DramCacheBlockInfo* create();
	void invalidate(void);
	bool isValid() const { return (m_tag != ((IntPtr) ~0)); }
	IntPtr getTag() const { return m_tag; }
	CacheState::cstate_t getCState() const { return m_cstate; }
	UInt32 getValidBits() const { return m_vbit; }
	UInt32 getDirtyBits() const { return m_dbit; }

	void setTag(IntPtr tag) { m_tag = tag; }
	void setCState(CacheState::cstate_t cstate) { m_cstate = cstate; }
	void setValidBit(UInt32 v) { m_vbit = v; }
	void setDirtyBit(UInt32 d) { m_dbit = d; }
};

class DramCacheSetAlloy
{
public:
	DramCacheSetAlloy(UInt32 associativity, UInt32 blocksize);
	~DramCacheSetAlloy();

	UInt32 getReplacementIndex();
	void updateReplacementIndex(UInt32);
	void updateReplacementIndexTag(UInt32 index, IntPtr tag);

	UInt8 accessAttempt(Core::mem_op_t type, IntPtr tag, IntPtr offset);
	
	int reads;
	int writes;

	DramCacheBlockInfo** m_cache_block_info_array;
protected:
	const UInt32 m_associativity;
};

class StackDramCacheCntlrAlloy
{
public:
	ComponentBandwidth m_dram_bandwidth;

	struct DramStats {
		int reads;
		int writes;
		double misses;
	} dram_stats;

	DramCacheSetAlloy** m_set_array;
	
	UInt32 m_set_num;
	UInt32 m_associativity;
	UInt32 m_blocksize;

	UInt32 m_vault_num;
	UInt32 m_vault_size;
	UInt32 m_bank_size;
	UInt32 m_row_size;
	
	//Performance model
	StackedDramPerfAlloy* m_dram_perf_model;
	
	StackDramCacheCntlrAlloy(UInt32 set_num, UInt32 associativity, UInt32 blocksize);
	~StackDramCacheCntlrAlloy();

	SubsecondTime ProcessRequest(SubsecondTime pkt_time, DramCntlrInterface::access_t access_type, IntPtr address);
	bool SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *block_tag, IntPtr *block_offset);
	friend class DramPerfModelAlloy;
};

class DramPerfModelAlloy : public DramPerfModel
{
   private:
      QueueModel* m_queue_model;
      TimeDistribution* m_dram_access_cost;
      ComponentBandwidth m_dram_bandwidth;

      SubsecondTime m_total_queueing_delay;
      SubsecondTime m_total_access_latency;

	  StackDramCacheCntlrAlloy* m_dram_cache_cntlr;

   public:
      DramPerfModelAlloy(core_id_t core_id,
            UInt32 cache_block_size);

      ~DramPerfModelAlloy();

      SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf);
};


#endif
