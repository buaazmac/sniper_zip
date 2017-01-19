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

void
DramCachePageInfo::invalidate()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
	vbits_0 = ~0;
	vbits_1 = ~0;
	dbits_0 = 0;
	dbits_1 = 0;
	m_used = 0;
	offset = 0;
	m_footprint = 0;
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
	vbits_0 = vbits_0 | (1UL << (block_num - 1));
	// set block dirty if write operation
	if (type == Core::WRITE) {
		dbits_0 = dbits_0 | (1UL << (block_num - 1));
	}
	// set block footprint
	m_footprint = m_footprint | (1UL << (block_num - 1));
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

void
DramCacheSetUnison::invalidateContent()
{
	for (UInt32 i = 0; i < m_associativity; i++) {

		m_cache_page_info_array[i]->invalidate();

	}
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

	m_dram_perf_model = new StackedDramPerfUnison(m_vault_num, m_vault_size, m_bank_size, m_row_size);
	Sim()->getStatsManager()->init_stacked_dram_unison(m_dram_perf_model);
}

StackDramCacheCntlrUnison::~StackDramCacheCntlrUnison()
{
	std::cout << "---deleting dram cache controller from NORMAL model" << std::endl;

	std::ofstream myfile;
	myfile.open("DramCacheAccess1.txt");

	myfile << "Simulation start" << std::endl;

	UInt32 tot_access = 0, tot_miss = 0;
	float miss_rate;

	for (UInt32 i = 0; i < 32; i++) {
		UInt32 i_access = dram_stats[i].reads + dram_stats[i].writes;
		if (i_access != 0) 
			miss_rate = (float)dram_stats[i].misses / (float)i_access;
		else
			miss_rate = 0;
		myfile << "dram_cache_" << i << ", access: " << i_access 
			<< ", miss: " << dram_stats[i].misses << ", miss rate: " << miss_rate 
			<< std::endl;
		tot_access += i_access;
		tot_miss += dram_stats[i].misses;
	}

	if (tot_access != 0) 
		miss_rate = (float)tot_miss / (float)tot_access;
	else
		miss_rate = 0;
	myfile << "dram_cache_total access: " << tot_access 
		<< ", miss: " << tot_miss << ", miss rate: " << miss_rate 
		<< std::endl;
	myfile.close();

	std::cout << "[EXTRA OUTPUT] DRAM Total Access: " << tot_access 
		<< ", miss: " << tot_miss << ", miss rate: " << miss_rate 
		<< std::endl;

	log_file.close();
	delete m_set_info;
	delete [] m_set;

	delete m_dram_perf_model;
}

SubsecondTime
StackDramCacheCntlrUnison::ProcessRequest(SubsecondTime pkt_time, DramCntlrInterface::access_t access_type, IntPtr address)
{
	SubsecondTime model_delay = SubsecondTime::Zero();
	SubsecondTime dram_delay = SubsecondTime::Zero();
	UInt32 set_n;
	IntPtr page_tag;
	IntPtr page_offset;
	UInt64 pc = 0;
	UInt32 footprint = 0;

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

	UInt8 hit = m_set[set_n]->accessAttempt(mem_op_type, page_tag, page_offset);

	// access tag need a read
	dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, set_n, DramCntlrInterface::READ); 

	if (hit == 0) { // page is not in cache
		m_set[set_n]->reads++;
		UInt32 index = m_set[set_n]->getReplacementIndex();
		// page eviction
		/*
		   1. alter page tag
		   2. load footprint blocks: set vbits & dbits
		   3. set pc & offset bits
		   4. update footprint history table
		   */
		dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, set_n, DramCntlrInterface::WRITE); 

		// page eviction & load new page
		m_set[set_n]->updateReplacementIndexTag(index, page_tag, pc, page_offset, footprint);

		// update footprint history table
		UInt32 new_footprint = m_set[set_n]->m_cache_page_info_array[index]->getFootprint();
		FHT[block_num] = new_footprint;

		// calculate model delay (page load): load a page (1 page = 1984 B)
		// 1 write = 2 read
		//model_delay += m_dram_bandwidth.getRoundedLatency(1984);
		//model_delay += m_dram_bandwidth.getRoundedLatency(1984);
		model_delay += m_dram_bandwidth.getRoundedLatency(8 * 2 * 1024);
	} else if (hit == 1) { // page is in cache, but block is not
		// we need to load block from memory
		// we also need to update page's footprint
		
		// calculate model delay (block load): load a block
	    //model_delay += m_dram_bandwidth.getRoundedLatency(64);
	    //model_delay += m_dram_bandwidth.getRoundedLatency(64);
		dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, set_n, DramCntlrInterface::WRITE); 

		model_delay += m_dram_bandwidth.getRoundedLatency(8 * 32);
	} 

	dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, set_n, access_type); 

	model_delay += dram_delay;

	UInt32 vault_bit = floorLog2(m_vault_num);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);
	UInt32 vault_i = (set_n >> row_bit) & ((1UL << vault_bit) - 1);
	UInt32 bank_i = (set_n >> row_bit >> vault_bit) & ((1 << bank_bit) - 1);

#ifdef ADDR_LOG
	UInt32 hit_i32 = hit;
	log_file << "-address: " << address
			 << ", set: " << set_n
			 << ", page_tag: " << page_tag
			 << ", page_offset: " << page_offset 
			 << ", hit: " << hit_i32
			 << std::endl;
#endif

	if (access_type == DramCntlrInterface::WRITE) {
		dram_stats[vault_i].writes ++;
		if (hit == 0 || hit == 1) {
			dram_stats[vault_i].misses ++;
		}
	} else {
		dram_stats[vault_i].reads ++;
		if (hit == 0 || hit == 1) {
			dram_stats[vault_i].misses ++;
		}
	}

	/* Check vaults valid bit*/

	m_dram_perf_model->checkTemperature(vault_i, bank_i);

	bool v_valid_arr[32];
	int b_valid_arr[32];
	m_dram_perf_model->checkDramValid(v_valid_arr, b_valid_arr);
	/*
	for (UInt32 i = 0; i < 32; i++) {
		v_valid_arr[i] = false;
	}
	*/
	//v_valid_arr[vault_i] = false;

	for (UInt32 i = 0; i < 32; i++) {
		UInt32 row_num = (1 << row_bit);
		UInt32 bank_num = (1 << bank_bit);
		if (v_valid_arr[i]) {
			int b_valid = b_valid_arr[i];
			for (UInt32 bank_i = 0; bank_i < bank_num; bank_i++) {
				if (((b_valid >> bank_i) & 1) == 0) {
					#define INVALID_LOG
					#ifdef INVALID_LOG
					log_file << "INVALID BANK #" << i << "_" << bank_i << std::endl;
					#endif
					for (UInt32 row_i = 0; row_i < row_num; row_i++) {
						UInt32 set_i = bank_i << vault_bit << row_bit;
						set_i |= (i << row_bit);
						set_i |= row_i;
						m_set[set_i]->invalidateContent();
					}
				}
			}
		} else {
			
			//std::cout << "Core0's Power: " << Sim()->getStatsManager()->getMetricObject("core", 0, "power-dynamic")->recordMetric() << std::endl;
			//std::cout << "Core0's renaming unit Power: " << Sim()->getStatsManager()->getMetricObject("ru", 0, "power-dynamic")->recordMetric() << std::endl;

			//std::cout << "Core0's Power: " << Sim()->getStatsManager()->getMetricObject("core", 0, "energy-dynamic")->recordMetric() << std::endl;

			#define INVALID_LOG
			#ifdef INVALID_LOG
			log_file << "INVALID VAULT #" << i << std::endl;
			#endif
			/* we need to invalidate all sets in the vaults*/
			for (UInt32 row_i = 0; row_i < row_num; row_i ++) {
				for (UInt32 bank_i = 0; bank_i < bank_num; bank_i ++) {
					UInt32 set_i = bank_i << vault_bit << row_bit;
					set_i |= (i << row_bit);
					set_i |= row_i;
					m_set[set_i]->invalidateContent();
				}
			}
		}
	}

	m_dram_perf_model->finishInvalidation();

	/**/

	return model_delay;
}

bool
StackDramCacheCntlrUnison::SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *page_tag, IntPtr *page_offset)
{
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
	SubsecondTime model_delay = SubsecondTime::Zero();
	model_delay = m_dram_cache_cntlr->ProcessRequest(pkt_time, access_type, address);

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

   SubsecondTime access_latency =  model_delay;


   perf->updateTime(pkt_time);
   perf->updateTime(pkt_time, ShmemPerf::DRAM_QUEUE);
   perf->updateTime(pkt_time + access_latency);

   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += access_latency;

   return access_latency;
}
