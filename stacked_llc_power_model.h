#ifndef __STACKED_LLC_H__
#define __STACKED_LLC_H__

#define STACKED_LLC

#include "core.h"
#include "cache.h"
#include "prefetcher.h"
#include "shared_cache_block_info.h"
#include "address_home_lookup.h"
#include "../pr_l1_pr_l2_dram_directory_msi/shmem_msg.h"
#include "mem_component.h"
#include "semaphore.h"
#include "lock.h"
#include "setlock.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "contention_model.h"
#include "req_queue_list_template.h"
#include "stats.h"
#include "subsecond_time.h"

#include "dram_cntlr_interface.h"

#include <iostream>
#include <fstream>

#include "fixed_types.h"

#include "cache_set_page_lru.h"


   class StackedDramCacheCntlr
   {
	   public:

		   struct DramStats {
			   int reads;
			   int writes;
		   } dram_stats[32][4];

		   CacheSetInfoPageLRU* m_set_info;
		   CacheSetPageLRU** m_set;

		   UInt32 m_set_num;
		   UInt32 m_associativity;
		   UInt32 m_blocksize;
		   UInt32 m_pagesize;
		   
		   StackedDramCacheCntlr(UInt32 set_num, UInt32 associativity, UInt32 blocksize, UInt32 pagesize);
		   ~StackedDramCacheCntlr();

		   void ProcessRequest(Core::mem_op_t mem_op_type, IntPtr address);
		   bool SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *page_tag);

		friend class CacheCntlr;
   };

class StackedLlcPowerModel {
	private:
		UInt32 cache_size;
		UInt32 num_layers;
		UInt32 num_sets;
		UInt32 associativity;
		
		StackedDramVault* vaults[VAULT_NUM];
	
	public:
		StackedLlcPowerModel(UInt32 size, UInt32 sets, UInt32 assoc);
		~StackedLlcPowerModel();
};

class StackedCacheSetPowerModel {
	private:
		int reads;
		int writes;
		int vault_n;
		int part_n;
		
};

class StackedDramCache {
};

#endif
