#include "dram_perf_model_normal.h"
#include "simulator.h"
#include "config.h"
#include "config.hpp"
#include "stats.h"
#include "shmem_perf.h"

#include <iostream>
#include <fstream>

/*
   Dram Cache Page Info Class
   */


DramCachePageInfo::DramCachePageInfo()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
	m_used = 0;
	pc = 0;
	offset = 0;

	vbits_0 = ~0;
	vbits_1 = ~0;
	dbits_0 = 0;
	dbits_1 = 0;

	m_footprint = 0;
}

DramCachePageInfo::DramCachePageInfo(IntPtr tag, CacheState::cstate_t cstate)
	: m_tag(tag),
	  m_cstate(cstate),
	  m_used(0)
{}

DramCachePageInfo::~DramCachePageInfo()
{}

UInt32
DramCachePageInfo::invalidate()
{
	UInt32 wb_blocks = 0;
	while (dbits_0 > 0) {
		if ((dbits_0 & 1) == 1)
			wb_blocks++;
		dbits_0 >>= 1;
	}
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
	vbits_0 = ~0;
	vbits_1 = ~0;
	dbits_0 = 0;
	dbits_1 = 0;
	m_used = 0;
	offset = 0;
	m_footprint = 0;
	return wb_blocks;
}

void
DramCachePageInfo::clone(DramCachePageInfo* cache_page_info)
{
	m_tag = cache_page_info->getTag();
	m_cstate = cache_page_info->getCState();
	m_used = cache_page_info->getUsage();
}

bool
DramCachePageInfo::updateUsage(UInt32 offset, UInt32 size)
{
	return true;
}

bool
DramCachePageInfo::updateUsage(UInt32 used)
{
	m_used = used;
	return true;
}

bool
DramCachePageInfo::accessBlock(Core::mem_op_t type, UInt32 block_num)
{
	bool v0 = (vbits_0 >> (block_num - 1)) & 1,
		 v1 = (vbits_1 >> (block_num - 1)) & 1,
		 d0 = (dbits_0 >> (block_num - 1)) & 1,
		 d1 = (dbits_1 >> (block_num - 1)) & 1;

	// set block valid anyway: load block if miss
	vbits_0 = vbits_0 | (1UL << block_num);
	// set block dirty if write operation
	if (type == Core::WRITE) {
		dbits_0 = dbits_0 | (1UL << block_num);
	}
	// set block footprint
	m_footprint = m_footprint | (1UL << block_num);
	if (v1 || d0 || d1) {};

	if (v0) return true;
	return false;
}

void
DramCachePageInfo::loadBlocks(UInt32 footprint)
{
	m_footprint = 0;
	vbits_0 = footprint;
}

/*
   DramCacheSetInfoUnison Class
   */
DramCacheSetInfoUnison::DramCacheSetInfoUnison(UInt32 associativity)
	: m_associativity(associativity)
{
}

DramCacheSetInfoUnison::~DramCacheSetInfoUnison()
{
}

/* 
   DramCacheSetUnison class
   */
DramCacheSetUnison::DramCacheSetUnison(UInt32 associativity, UInt32 blocksize, 
				DramCacheSetInfoUnison* set_info)
	: reads(0),
	  writes(0),
	  m_set_info(set_info),
	  m_associativity(associativity)
{
	m_lru_bits = new UInt8[m_associativity];
	for (UInt32 i = 0; i < m_associativity; i++) {
		m_lru_bits[i] = i;
	}

	m_cache_page_info_array = new DramCachePageInfo*[m_associativity];
	for (UInt32 i = 0; i < m_associativity; i++) {
		m_cache_page_info_array[i] = new DramCachePageInfo();
	}
}

DramCacheSetUnison::~DramCacheSetUnison()
{
	for (UInt32 i = 0; i < m_associativity; i++) {
		delete m_cache_page_info_array[i];
	}
	delete [] m_lru_bits;
	delete [] m_cache_page_info_array;
}

UInt32
DramCacheSetUnison::getReplacementIndex()
{
	reads++;
	UInt8 max_t = 0;
	UInt32 index = 0;
	for (UInt32 i = 0; i < m_associativity; i++) {
		if (m_cache_page_info_array[i]->getUsage() > max_t) {
			max_t = m_cache_page_info_array[i]->getUsage();
			index = i;
		}
	}
	return index;
}

UInt32
DramCacheSetUnison::invalidateContent()
{
	//std::cout << "before invalidate content" << std::endl;
	UInt32 wb_blocks = 0;
	for (UInt32 i = 0; i < m_associativity; i++) {
		wb_blocks += m_cache_page_info_array[i]->invalidate();
	}
	//std::cout << "end invalidate content" << std::endl;
	return wb_blocks;
}

UInt32
DramCacheSetUnison::getDirtyBlocks()
{
	UInt32 dirty_blocks = 0;
	for (UInt32 i = 0; i < m_associativity; i++) {
		UInt32 db = m_cache_page_info_array[i]->getDirtyBits();
		while (db > 0) {
			if ((db & 1) == 1) {
				dirty_blocks ++;
			}
			db >>= 1;
		}
	}
	//std::cout << "after get valid blocks" << std::endl;
	return dirty_blocks;
}

UInt32
DramCacheSetUnison::getValidBlocks()
{
	//std::cout << "before get valid blocks" << std::endl;
	UInt32 valid_blocks = 0;
	for (UInt32 i = 0; i < m_associativity; i++) {
		UInt32 vb = m_cache_page_info_array[i]->getValidBits();
		while (vb > 0) {
			if ((vb & 1) == 1) {
				valid_blocks ++;
			}
			vb >>= 1;
		}
	}
	//std::cout << "after get valid blocks" << std::endl;
	return valid_blocks;
}

void
DramCacheSetUnison::updateReplacementIndex(UInt32 index)
{
}

void
DramCacheSetUnison::updateReplacementIndexTag(UInt32 index, IntPtr tag, IntPtr pc, 
		IntPtr offset, UInt32 footprint)
{
	DramCachePageInfo *page = m_cache_page_info_array[index];
	// here we simulate the replacement process
	page->setTag(tag);
	page->setValidBits(footprint);
	page->setDirtyBits(0);
	page->setFootprint(0);

	//increase the stats
	//reads++
	writes++;
	
	updateUsedInfo(tag);
}

void
DramCacheSetUnison::updateUsedInfo(IntPtr tag)
{
	UInt32 index = m_associativity;
	for (UInt32 i = 0; i < m_associativity; i++) {
		if (m_cache_page_info_array[i]->getTag() == tag) {
			index = i;
			break;
		}
	}
	if (index == m_associativity) {
		std::cout << "ERROR: update tag" << std::endl;
		return;
	} else {
		if (m_cache_page_info_array[index]->getUsage() != 0) {
			for (UInt32 i = 0; i < m_associativity; i++) {
				if (index == i) {
					m_cache_page_info_array[i]->updateUsage(0);
				} else {
					UInt8 tmp = m_cache_page_info_array[i]->getUsage();
					m_cache_page_info_array[i]->updateUsage(tmp + 1);
				}
			}
		}
	}
}

UInt8
DramCacheSetUnison::accessAttempt(Core::mem_op_t type, IntPtr tag, IntPtr offset)
{
	UInt8 res = 0;
	UInt32 block_num = offset / StackedBlockSize;
	// here we simulate the looking up process
	reads++;
	for (UInt32 i = 0; i < m_associativity; i++) {
		DramCachePageInfo *page = m_cache_page_info_array[i];
		if (page->getTag() == tag) {

			if (page->accessBlock(type, block_num)) {
				
				res = 2;
			} else {
				res = 1;
				// reload block
				writes++;
			}
			break;
		}
	}
	return res;
}

/*
	StackDramCacheCntlrUnison class
   */
StackDramCacheCntlrUnison::StackDramCacheCntlrUnison(
		UInt32 set_num, UInt32 associativity, UInt32 blocksize, UInt32 pagesize)
      : m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")), // Convert bytes to bits
	  m_set_num(set_num),
	  m_associativity(associativity),
	  m_blocksize(blocksize),
	  m_pagesize(pagesize)
{
	log_file.open("unison_addr.txt");

	std::cout << "Normal Cache Total set: " << set_num << std::endl;
	m_set_info = new DramCacheSetInfoUnison(m_associativity);

	m_set = new DramCacheSetUnison*[m_set_num];

	for (UInt32 i = 0; i < 31; i++) {
		FHT[i] = 0;
	}

	for (UInt32 i = 0; i < m_set_num; i++) {
		UInt32 offset = i >> 12;
		UInt32 vault_mask = (1ull << 5) - 1;
		m_set[i] = new DramCacheSetUnison(m_associativity, m_blocksize, m_set_info);

		m_set[i]->n_vault = offset & vault_mask;
		m_set[i]->n_bank = (i >> 11) & 1;
		m_set[i]->n_level = (offset >> 5) & 3;
	}

	for (UInt32 i = 0; i < 32; i++) {
		dram_stats[i].reads = 0;
		dram_stats[i].writes = 0;
		dram_stats[i].misses = 0;
	}

	// Initialize dram performance model
	// we have 32 vaults, 1 vault = 128 mb, 1 bank = 16 mb, 1 row = 8 kb
	m_vault_num = 32;
	m_vault_size = 128 * 1024;
	m_bank_size = 16 * 1024;
	m_row_size = 8;
	// Statistics for simulating memory operation
	cache_access = page_misses = block_misses = wb_blocks = ld_blocks = 0;
	// Statistics for simulating remapping
	invalid_times = invalid_blocks = migrate_times = migrate_blocks = 0;
	// Choose Invalidation/Migration mechanism
	remap_invalid = true;

	m_dram_perf_model = new StackedDramPerfUnison(m_vault_num, m_vault_size, m_bank_size, m_row_size);
	/*
	   Initial DRAM stats
	 */
	Sim()->getStatsManager()->init_stacked_dram_unison(m_dram_perf_model);

	/*
	   Initial Page Table
	*/
	avail_phy_page_tag = 0;

}

StackDramCacheCntlrUnison::~StackDramCacheCntlrUnison()
{
	float miss_rate = 0;
	UInt32 tot_miss = page_misses + block_misses;

	if (cache_access != 0) 
		miss_rate = (float)tot_miss / (float)cache_access;

	UInt32 cache_access_roi = cache_access - cache_access_no_roi,
		   tot_miss_roi = tot_miss - page_misses_no_roi - block_misses_no_roi;
	float miss_rate_roi = 0;
	if (cache_access_roi != 0)
		miss_rate_roi = (float)tot_miss_roi / (float)cache_access_roi;

	std::cout << "\n ***** [DRAM_CACHE_Result] *****\n\n";
	std::cout << "*** DRAM Total Access: " << cache_access 
			  << ", miss: " << tot_miss << ", miss rate: " << miss_rate 
			  << std::endl;
	std::cout << "*** DRAM Total Access In ROI: " << cache_access_roi
			  << ", miss: " << tot_miss_roi << ", miss rate: " << miss_rate_roi
			  << std::endl;
	std::cout << "*** DRAM Remap Times: " 
			  << invalid_times << " invalid times, " << invalid_blocks << " total invalid_blocks, "
			  << migrate_times << " migrate times, " << migrate_blocks << " total migrate blocks."
			  << std::endl;
	std::cout << "*** DRAM Statistics: " 
			  << m_dram_perf_model->tot_dram_reads << " reads, "
			  << m_dram_perf_model->tot_dram_writes << " writes, "
			  << m_dram_perf_model->tot_row_hits << " row hits, "
			  << m_dram_perf_model->tot_act_t.getUS() << " ACT time, "
			  << m_dram_perf_model->tot_pre_t.getUS() << " PRE time, "
			  << m_dram_perf_model->tot_rd_t.getUS() << " RD time, "
			  << m_dram_perf_model->tot_wr_t.getUS() << " WR time."
			  << std::endl;
	std::cout << "\n ***** [DRAM_CACHE_Result] *****\n\n";

	log_file.close();
	delete m_set_info;
	delete [] m_set;

	delete m_dram_perf_model;
}

SubsecondTime
StackDramCacheCntlrUnison::ProcessRequest(SubsecondTime pkt_time, DramCntlrInterface::access_t access_type, IntPtr address, ShmemPerf *perf)
{
	SubsecondTime model_delay = SubsecondTime::Zero();
	SubsecondTime dram_delay = SubsecondTime::Zero();
	SubsecondTime mem_access_delay = SubsecondTime::Zero();
	UInt32 set_n;
	IntPtr page_tag;
	IntPtr page_offset;
	UInt64 pc = 0;
	UInt32 footprint = 0;

	/* Here we need to translate virtual address to physical adress
TODO:
	 * We can use a map, to map virtual page to phsical page
	 * Page Size: 2KB, We have 4GB physical memory
	 * ----------> We have 2M physical pages
	 */

	if (SplitAddress(address, &set_n, &page_tag, &page_offset)) {
	} else {
		std::cout << "ERROR: process request from llc" << std::endl;
		return model_delay;
	}


	UInt8 block_num = page_offset / m_blocksize;
	footprint = FHT[block_num];

	Core::mem_op_t mem_op_type;
	if (access_type == DramCntlrInterface::WRITE) 
		mem_op_type = Core::WRITE;
	else
		mem_op_type = Core::READ;

	/* Place to call remapping manager (REMAP_MAN) */
	SubsecondTime remap_delay = checkRemapping(pkt_time, perf);

	/* Try to acccess cache set*/
	UInt8 hit = m_set[set_n]->accessAttempt(mem_op_type, page_tag, page_offset);
	cache_access ++;


	/*
	   
	*/

	// access tag need a read
	//dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 64, set_n, DramCntlrInterface::READ); 
	dram_delay += handleDramAccess(pkt_time, 64, set_n, DramCntlrInterface::READ, perf);

	/* Here we record how many load/write to main memory after cache */
	UInt32 load_blocks = 0, writeback_blocks = 0;

	if (hit == 0) { // page is not in cache
		page_misses ++;

		m_set[set_n]->reads++;
		UInt32 index = m_set[set_n]->getReplacementIndex();
		// page eviction
		/*
		   1. alter page tag
		   2. load footprint blocks: set vbits & dbits
		   3. set pc & offset bits
		   4. update footprint history table
		   */

		// update footprint history table
		UInt32 new_footprint = m_set[set_n]->m_cache_page_info_array[index]->getFootprint();
		FHT[block_num] = new_footprint;
		while (new_footprint > 0) {
			if ((new_footprint & 1) == 1) {
				load_blocks++;
			}
			new_footprint >>= 1;
		}
		// page eviction & load new page
		writeback_blocks = m_set[set_n]->m_cache_page_info_array[index]->invalidate();
		wb_blocks += writeback_blocks;
		ld_blocks += load_blocks;

		m_set[set_n]->updateReplacementIndexTag(index, page_tag, pc, page_offset, footprint);

		/* Write Back Dirty Blocks*/
		//dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 64 * writeback_blocks, set_n, DramCntlrInterface::WRITE); 
		dram_delay += handleDramAccess(pkt_time, 64 * writeback_blocks, set_n, DramCntlrInterface::WRITE, perf); 
		dram_delay += handleDramAccess(pkt_time, 64 * writeback_blocks, set_n, DramCntlrInterface::READ, perf); 
		/* Load New Blocks from Memory*/

		// calculate model delay (page load): load a page (1 page = 1984 B)
		// 1 write = 2 read
		//model_delay += m_dram_bandwidth.getRoundedLatency(1984);
		//model_delay += m_dram_bandwidth.getRoundedLatency(1984);
		model_delay += m_dram_bandwidth.getRoundedLatency(8 * 2 * 1024);
	} else if (hit == 1) { // page is in cache, but block is not
		block_misses ++;
		// we need to load block from memory
		// we also need to update page's footprint
		
		// calculate model delay (block load): load a block
	    //model_delay += m_dram_bandwidth.getRoundedLatency(64);
	    //model_delay += m_dram_bandwidth.getRoundedLatency(64);
		//dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 64, set_n, DramCntlrInterface::WRITE); 
		dram_delay += handleDramAccess(pkt_time, 64, set_n, DramCntlrInterface::WRITE, perf); 

		load_blocks ++;
		//model_delay += m_dram_bandwidth.getRoundedLatency(8 * 64);
	} 

	//dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 64, set_n, access_type); 
	dram_delay += handleDramAccess(pkt_time, 64, set_n, access_type, perf); 

	/* Here we update memory access delays*/
	mem_access_delay += m_dram_bandwidth.getRoundedLatency(8 * 64 * load_blocks);
	mem_access_delay += m_dram_bandwidth.getRoundedLatency(8 * 64 * writeback_blocks);


	UInt32 vault_bit = floorLog2(m_vault_num);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);
	UInt32 vault_i = (set_n >> row_bit) & ((1UL << vault_bit) - 1);
	UInt32 bank_i = (set_n >> row_bit >> vault_bit) & ((1 << bank_bit) - 1);


	if (access_type == DramCntlrInterface::WRITE) {
		m_dram_perf_model->tot_writes ++;
		dram_stats[vault_i].writes ++;
		if (hit == 0 || hit == 1) {
			m_dram_perf_model->tot_misses ++;
			dram_stats[vault_i].misses ++;
		}
	} else {
		m_dram_perf_model->tot_reads ++;
		dram_stats[vault_i].reads ++;
		if (hit == 0 || hit == 1) {
			m_dram_perf_model->tot_misses ++;
			dram_stats[vault_i].misses ++;
		}
	}

	/* Check vaults valid bit*/
	
	//m_dram_perf_model->checkTemperature(vault_i, bank_i);

	model_delay += dram_delay;
	model_delay += remap_delay;
	model_delay += mem_access_delay;
	/**/
	Sim()->getStatsManager()->updateCurrentTime(pkt_time + model_delay);

	return model_delay;
}

SubsecondTime
StackDramCacheCntlrUnison::checkRemapping(SubsecondTime pkt_time, ShmemPerf *perf)
{
	UInt32 vault_bit = floorLog2(m_vault_num);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);
	UInt32 row_num = (1 << row_bit);
	UInt32 bank_num = (1 << bank_bit);
	UInt32 vault_num = m_vault_num;

	if (m_dram_perf_model->enter_roi == false) {
		cache_access_no_roi = cache_access;
		page_misses_no_roi = page_misses;
		block_misses_no_roi = block_misses;
	}

	SubsecondTime dram_delay = SubsecondTime::Zero();
	if (m_dram_perf_model->remapped == false) {
		return dram_delay;
	}

	/* REMAP_DEBUG */
	//std::cout << "[REMAP_DEBUG] Here we found a remapping happened" << std::endl;

	m_dram_perf_model->remapped = false;

	//m_dram_perf_model->checkStat(vault_i, bank_i);
	//m_dram_perf_model->checkDramValid(v_valid_arr, b_valid_arr, b_migrated_arr);
	UInt32 writeback_blocks = 0, valid_blocks = 0;

	for (UInt32 vault_i = 0; vault_i < vault_num; vault_i++) {
		for (UInt32 bank_i = 0; bank_i < bank_num; bank_i++) {
			for (UInt32 row_i = 0; row_i < row_num; row_i++) {
				bool valid = m_dram_perf_model->checkRowValid(vault_i, bank_i, row_i),
					 migrated = m_dram_perf_model->checkRowMigrated(vault_i, bank_i, row_i);
				if (valid && !migrated) continue;

				UInt32 set_valid_blocks = 0, set_wb_blocks = 0;
				UInt32 set_i = m_dram_perf_model->getSetNum(vault_i, bank_i, row_i);

				set_wb_blocks = m_set[set_i]->getDirtyBlocks();
				set_valid_blocks = m_set[set_i]->getValidBlocks();

				if (set_wb_blocks < 10 && set_wb_blocks < set_valid_blocks) {
					/* Latency for invalidation */
					dram_delay += m_dram_bandwidth.getRoundedLatency(64 * set_wb_blocks);
					dram_delay += handleDramAccess(pkt_time, set_wb_blocks * 64, set_i, DramCntlrInterface::READ, perf); 
					m_set[set_i]->invalidateContent();
					invalid_times ++;
					invalid_blocks += set_wb_blocks;
				} else {
					/* Latency for migration */
					dram_delay += handleDramAccess(pkt_time, 64, set_i, DramCntlrInterface::READ, perf); 
					dram_delay += handleDramAccess(pkt_time, 64, set_i, DramCntlrInterface::WRITE, perf); 
					dram_delay += handleDramAccess(pkt_time, set_valid_blocks * 64, set_i, DramCntlrInterface::TRANS, perf); 
					dram_delay += handleDramAccess(pkt_time, set_valid_blocks * 64, set_i, DramCntlrInterface::TRANS, perf); 
					migrate_times ++;
					migrate_blocks += set_valid_blocks;
				}
			

				std::cout << "[REMAP_DEBUG] Amazing, we found an invalid row!\n"
					      << "------wb_blocks: " << set_wb_blocks
						  << "------valid blocks: " << set_valid_blocks
						  << std::endl;
				
				valid_blocks += set_valid_blocks;
				writeback_blocks += set_wb_blocks;
			}
		}
	}
	m_dram_perf_model->clearRemappingStat();
	//m_dram_perf_model->updateStats();

	wb_blocks += writeback_blocks;
	
	return dram_delay;
}

SubsecondTime
StackDramCacheCntlrUnison::handleDramAccess(SubsecondTime pkt_time, UInt32 pkt_size, UInt32 set_n, DramCntlrInterface::access_t access_type, ShmemPerf *perf) {
	UInt32 bandwidth = 128;
	int req_times = pkt_size / bandwidth;
	if (req_times < 1) {
		req_times = 1;
	}
	if (pkt_size == 0)
		req_times = 0;

	/*
	std::cout << "[Handle Dram Access] " 
			  << "pkt_time: " << pkt_time
			  << ", pkt_size: " << pkt_size << std::endl;
			  */

	SubsecondTime delay = SubsecondTime::Zero();
	while (req_times--) {
		delay += m_dram_perf_model->getAccessLatency(pkt_time + delay, 128, set_n, access_type); 

		Sim()->getStatsManager()->updateCurrentTime(pkt_time + delay);
		perf->updateTime(pkt_time, ShmemPerf::DRAM_QUEUE);
		perf->updateTime(pkt_time + delay);
	}
	return delay;
}

IntPtr
StackDramCacheCntlrUnison::translateAddress(IntPtr address)
{
	IntPtr offset = address & ((1 << 12) - 1);
	IntPtr v_tag = address >> 12;
	IntPtr p_tag = v_tag;
	
	if (page_table.find(v_tag) != page_table.end()) {
		p_tag = page_table[v_tag];
	} else {
		page_table[v_tag] = avail_phy_page_tag;
		p_tag = avail_phy_page_tag;
	}
	avail_phy_page_tag ++;
	return ((p_tag << 12) | offset);
}

bool
StackDramCacheCntlrUnison::SplitAddress(IntPtr v_address, UInt32 *set_n, IntPtr *page_tag, IntPtr *page_offset)
{
	IntPtr address = translateAddress(v_address);
	*page_offset = address % m_pagesize;
	UInt64 addr = address / m_pagesize;
	*set_n = addr & ((1UL << floorLog2(m_set_num)) - 1);
	*page_tag = addr >> floorLog2(m_set_num);
	return true;
}


/*
   --------------------------------------------------------------------------------
   */

DramPerfModelNormal::DramPerfModelNormal(core_id_t core_id,
      UInt32 cache_block_size):
   DramPerfModel(core_id, cache_block_size),
   m_queue_model(NULL),
   m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")), // Convert bytes to bits
   m_total_queueing_delay(SubsecondTime::Zero()),
   m_total_access_latency(SubsecondTime::Zero())
{
   SubsecondTime dram_latency = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/latency"))); // Operate in fs for higher precision before converting to uint64_t/SubsecondTime
   SubsecondTime dram_latency_stddev = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/normal/standard_deviation")));

   m_dram_access_cost = new NormalTimeDistribution(dram_latency, dram_latency_stddev);

   if (Sim()->getCfg()->getBool("perf_model/dram/queue_model/enabled"))
   {
      m_queue_model = QueueModel::create("dram-queue", core_id, Sim()->getCfg()->getString("perf_model/dram/queue_model/type"),
                                         m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
   }

   registerStatsMetric("dram", core_id, "total-access-latency", &m_total_access_latency);
   registerStatsMetric("dram", core_id, "total-queueing-delay", &m_total_queueing_delay);


   /*
	  Initialize a dram cache controller
	  */
   m_dram_cache_cntlr = new StackDramCacheCntlrUnison(StackedDramSize/StackedSetSize, StackedAssoc, StackedBlockSize, StackedPageSize);
}

DramPerfModelNormal::~DramPerfModelNormal()
{
   if (m_queue_model)
   {
     delete m_queue_model;
      m_queue_model = NULL;
   }
   delete m_dram_access_cost;
   delete m_dram_cache_cntlr;
}

SubsecondTime
DramPerfModelNormal::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
	//std::cout << "[DRAM_PERF_MODEL] before process a request\n";
	SubsecondTime model_delay = SubsecondTime::Zero();
	model_delay += m_dram_cache_cntlr->ProcessRequest(pkt_time, access_type, address, perf);
	//std::cout << "[DRAM_PERF_MODEL] after process a request\n";

	//printf("Normal Model# pkt_time: %ld, address: %ld\n", pkt_time.getMS(), address);
	/*
	std::ofstream myfile;
	myfile.open ("DRAM_Access_orig_1.txt", std::ofstream::out | std::ofstream::app);
	myfile << "Normal Model# pkt_time: " << pkt_time.getNS() << " address: " << address << std::endl;
	myfile.close();
	*/

   // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bits per clock cycle'
   if ((!m_enabled) ||
         (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
   {
      return SubsecondTime::Zero();
   }

   SubsecondTime access_latency = model_delay;

   //std::cout << "[OVERHEADS] Memory Request Latency: " << model_delay.getNS() << std::endl;

   perf->updateTime(pkt_time);
   perf->updateTime(pkt_time + model_delay, ShmemPerf::DRAM_QUEUE);
   perf->updateTime(pkt_time + model_delay, ShmemPerf::DRAM_BUS);
   perf->updateTime(pkt_time + model_delay, ShmemPerf::DRAM_DEVICE);

   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += access_latency;

   return access_latency;
}
