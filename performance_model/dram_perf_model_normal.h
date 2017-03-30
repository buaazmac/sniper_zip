#ifndef __DRAM_PERF_MODEL_NORMAL_H__
#define __DRAM_PERF_MODEL_NORMAL_H__

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
#include <map>

class DramCachePageInfo
{
	public:
		//page size = 1984B, Block size = 64B
		// 1 page has 31 blocks
		static const UInt32 BitsOfBlock = 5;

	private:
		IntPtr m_tag;

		UInt32 vbits_0;
		UInt32 vbits_1;
		UInt32 dbits_0;
		UInt32 dbits_1;

		UInt32 m_footprint;

		CacheState::cstate_t m_cstate;

		UInt8 m_used;
		UInt8 pc;
		UInt8 offset;

	public:
		DramCachePageInfo();
		DramCachePageInfo(IntPtr tag, CacheState::cstate_t cstate);

		~DramCachePageInfo();

		static DramCachePageInfo* create();

		UInt32 invalidate(void);
		void clone(DramCachePageInfo* cache_page_info);

		bool isValid() const { return (m_tag != ((IntPtr) ~0)); }

		IntPtr getTag() const { return m_tag; }
		CacheState::cstate_t getCState() const { return m_cstate; }
		UInt32 getValidBits() const { return vbits_0; }
		UInt32 getDirtyBits() const { return dbits_0; }
		UInt32 getFootprint() const { return m_footprint; }

		void setTag(IntPtr tag) { m_tag = tag; }
		void setCState(CacheState::cstate_t cstate) { m_cstate = cstate; }
		void setValidBits(UInt32 v) { vbits_0 = v; }
		void setDirtyBits(UInt32 d) { dbits_0 = d; }
		void setFootprint(UInt32 f) { m_footprint = f; }

		UInt8 getUsage() const { return m_used; }
		bool updateUsage(UInt32 offset, UInt32 size);
		bool updateUsage(UInt32 used);

		bool accessBlock(Core::mem_op_t, UInt32);

		void loadBlocks(UInt32 footprint);
};

class DramCacheSetInfoUnison
{
	public:
		DramCacheSetInfoUnison(UInt32 associativity);
		~DramCacheSetInfoUnison();
	private:
		const UInt32 m_associativity;
};

class DramCacheSetUnison
{
	public:
		DramCacheSetUnison(UInt32 associativity, UInt32 blocksize, 
				DramCacheSetInfoUnison* set_info);
		~DramCacheSetUnison();

		UInt32 getReplacementIndex();
		UInt32 invalidateContent();
		UInt32 getDirtyBlocks();
		UInt32 getValidBlocks();
		void updateReplacementIndex(UInt32);
		void updateReplacementIndexTag(UInt32 index, IntPtr tag, IntPtr pc, IntPtr offset, UInt32 footprint);

		void updateUsedInfo(IntPtr tag);

		UInt8 accessAttempt(Core::mem_op_t type, IntPtr tag, IntPtr offset);

		int reads;
		int writes;

		UInt32 n_vault;
		UInt32 n_bank;
		UInt32 n_level;

		DramCachePageInfo** m_cache_page_info_array;

	protected:
		UInt8* m_lru_bits;
		DramCacheSetInfoUnison* m_set_info;
		const UInt32 m_associativity;
		void moveToMRU(UInt32 accessed_index);
};

class StackDramCacheCntlrUnison
{
	public:
        ComponentBandwidth m_dram_bandwidth;
		
		struct DramStats {
			int reads;
			int writes;
			int misses;
		} dram_stats[32];

		DramCacheSetInfoUnison* m_set_info;
		DramCacheSetUnison** m_set;

		UInt32 FHT[31];	// footprint history table

		UInt32 m_set_num;
		UInt32 m_associativity;
		UInt32 m_blocksize;
		UInt32 m_pagesize;

		UInt32 m_vault_num;
		UInt32 m_vault_size;
		UInt32 m_bank_size;
		UInt32 m_row_size;

		UInt32 cache_access, page_misses, block_misses;
		UInt32 cache_access_no_roi, page_misses_no_roi, block_misses_no_roi;
		UInt32 wb_blocks, ld_blocks;
		UInt32 invalid_times, invalid_blocks, migrate_times, migrate_blocks;

		//log file
		std::ofstream log_file;

		//Performance model
		StackedDramPerfUnison* m_dram_perf_model;
		bool remap_invalid;
		

		StackDramCacheCntlrUnison(UInt32 set_num, UInt32 associativity, UInt32 blocksize, UInt32 pagesize);
		~StackDramCacheCntlrUnison();

		SubsecondTime ProcessRequest(SubsecondTime pkt_time, DramCntlrInterface::access_t access_type, IntPtr address, ShmemPerf *perf);
		SubsecondTime checkRemapping(SubsecondTime pkt_time, ShmemPerf *perf);

		SubsecondTime handleDramAccess(SubsecondTime pkt_time, UInt32 pkt_size, UInt32 set_n, DramCntlrInterface::access_t access_type, ShmemPerf *perf);

		/* Virtual-Physical Page Table*/
		UInt32 avail_phy_page_tag;
		std::map<IntPtr, IntPtr> page_table;
		IntPtr translateAddress(IntPtr address);

		bool SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *page_tag, IntPtr *page_offset);

		friend class DramPerfModelNormal;

};

class DramPerfModelNormal : public DramPerfModel
{
   private:
      QueueModel* m_queue_model;
      TimeDistribution* m_dram_access_cost;
      ComponentBandwidth m_dram_bandwidth;

      SubsecondTime m_total_queueing_delay;
      SubsecondTime m_total_access_latency;

	  StackDramCacheCntlrUnison* m_dram_cache_cntlr;

   public:
      DramPerfModelNormal(core_id_t core_id,
            UInt32 cache_block_size);

      ~DramPerfModelNormal();

      SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf);
};

#endif /* __DRAM_PERF_MODEL_NORMAL_H__ */
