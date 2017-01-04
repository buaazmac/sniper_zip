#include "dram_perf_model_alloy.h"
#include "simulator.h"
#include "config.h"
#include "config.hpp"
#include "stats.h"
#include "shmem_perf.h"

#include <iostream>
#include <fstream>

/*
   Dram Cache Block Info Class for AlloyCache
   */
DramCacheBlockInfo::DramCacheBlockInfo()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;

	m_vbit = ~0;
	m_dbit = 0;
}

DramCacheBlockInfo::DramCacheBlockInfo(IntPtr tag, CacheState::cstate_t cstate)
	: m_tag(tag),
	  m_cstate(cstate)
{}

DramCacheBlockInfo::~DramCacheBlockInfo()
{}

void
DramCacheBlockInfo::invalidate()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
}

/*
   DramCacheSetAlloy class
   */
DramCacheSetAlloy::DramCacheSetAlloy(UInt32 associativity, UInt32 blocksize)
	: reads(0),
	  writes(0),
	  m_associativity(associativity)
{
	m_cache_block_info_array = new DramCacheBlockInfo*[m_associativity];
	for (UInt32 i = 0; i < m_associativity; i++) {
		m_cache_block_info_array[i] = new DramCacheBlockInfo();
	}
}

DramCacheSetAlloy::~DramCacheSetAlloy()
{
	delete [] m_cache_block_info_array;
}

UInt32
DramCacheSetAlloy::getReplacementIndex()
{
	// Alloy cache is directed-map
	return 0;
}

void
DramCacheSetAlloy::updateReplacementIndexTag(UInt32 index, IntPtr tag)
{
	DramCacheBlockInfo *block = m_cache_block_info_array[index];
	block->setTag(tag);
	block->setValidBit(1);
	block->setDirtyBit(0);

	writes++;
}

UInt8
DramCacheSetAlloy::accessAttempt(Core::mem_op_t type, IntPtr tag, IntPtr offset)
{
	UInt8 res = 0;
	reads++;
	for (UInt32 i = 0; i < m_associativity; i++) {
		DramCacheBlockInfo *block = m_cache_block_info_array[i];
		if (block->getTag() == tag) {
			res = 1;
			break;
		}
	}
	return res;
}

/*
   StackDramCacheCntlr class
   */
StackDramCacheCntlrAlloy::StackDramCacheCntlrAlloy(
		UInt32 set_num, UInt32 associativity, UInt32 blocksize)
	: m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")),
	  m_set_num(set_num),
	  m_associativity(associativity),
	  m_blocksize(blocksize)
{
	std::cout << "Alloy Cache Total set: " << set_num << std::endl;
	m_set_array = new DramCacheSetAlloy*[m_set_num];

	for (UInt32 i = 0; i < m_set_num; i++) {
		m_set_array[i] = new DramCacheSetAlloy(m_associativity, m_blocksize);
	}

	// Initialize dram performance model
	// we have 32 vaults, 1 vault = 128 mb, 1 bank = 16 mb, 1 row = 8 kb
	m_vault_num = 32;
	m_vault_size = 128 * 1024;
	m_bank_size = 16 * 1024;
	m_row_size = 8;

	// Initialize Statistic Data
	dram_stats.reads = 0;
	dram_stats.writes = 0;
	dram_stats.misses = 0.0;

	m_dram_perf_model = new StackedDramPerfAlloy(m_vault_num, m_vault_size, m_bank_size, m_row_size);
	Sim()->getStatsManager()->init_stacked_dram_alloy(m_dram_perf_model);
}

StackDramCacheCntlrAlloy::~StackDramCacheCntlrAlloy()
{
	std::cout << "---deleting dram cache controller from ALLOY model" << std::endl;

	std::cout << "OUTPUT: DRAM Cache Stats: \n" 
			  << "\t access: " << dram_stats.reads + dram_stats.writes << std::endl
			  << "\t miss: " << dram_stats.misses << std::endl;

	delete [] m_set_array;

	delete m_dram_perf_model;
}

SubsecondTime
StackDramCacheCntlrAlloy::ProcessRequest(SubsecondTime pkt_time, DramCntlrInterface::access_t access_type, IntPtr address)
{
	SubsecondTime model_delay = SubsecondTime::Zero();
	SubsecondTime dram_delay = SubsecondTime::Zero();
	UInt32 set_n;
	IntPtr block_tag;
	IntPtr block_offset;

	// set_n is block_num because it's direct-mapped
	// block_tag is block tag
	// block_offset can be ignored
	if (SplitAddress(address, &set_n, &block_tag, &block_offset)) {
	} else {
		std::cout << "ERROR: process request from llc" << std::endl;
		return model_delay;
	}

	Core::mem_op_t mem_op_type;
	if (access_type == DramCntlrInterface::WRITE) 
		mem_op_type = Core::WRITE;
	else
		mem_op_type = Core::READ;

	UInt8 hit = m_set_array[set_n]->accessAttempt(mem_op_type, block_tag, block_offset);

	// access tag need a read
	// we put 28 TAD in one dram row
	UInt32 row_i = set_n / 28;
	dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, row_i, DramCntlrInterface::READ); 

	if (hit == 0) { // block is not in cache
		m_set_array[set_n]->reads++;
		UInt32 index = m_set_array[set_n]->getReplacementIndex();

		// block evict
		/*
		   1. alter block tag
		   */
		m_set_array[set_n]->updateReplacementIndexTag(index, block_tag);

		dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, row_i, DramCntlrInterface::WRITE);

		model_delay += m_dram_bandwidth.getRoundedLatency(8 * 2 * 1024);
	}


	dram_delay += m_dram_perf_model->getAccessLatency(pkt_time, 32, row_i, access_type); 

	model_delay += dram_delay;

	if (access_type == DramCntlrInterface::WRITE) {
		dram_stats.writes ++;
		if (hit == 0) {
			dram_stats.misses ++;
		}
	} else {
		dram_stats.reads ++;
		if (hit == 0) {
			dram_stats.misses ++;
		}
	}

	return model_delay;
}

bool
StackDramCacheCntlrAlloy::SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *block_tag, IntPtr *block_offset)
{
	*block_offset = address % m_blocksize;
	UInt64 addr = address / m_blocksize;
	*set_n = addr & ((1UL << floorLog2(m_set_num)) - 1);
	*block_tag = addr >> floorLog2(m_set_num);
	return true;
}

/*
   --------------------------------------------------------------------------------
   */

DramPerfModelAlloy::DramPerfModelAlloy(core_id_t core_id,
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
   std::cout << "StackedDramSize: " << StackedDramSize << " StackedBlockSize: " << StackedBlockSizeAlloy << std::endl;
   std::cout << StackedDramSize / StackedBlockSizeAlloy * 1024<< std::endl;
   m_dram_cache_cntlr = new StackDramCacheCntlrAlloy(StackedDramSize / StackedSetSize * 28, 1, StackedBlockSize);
}

DramPerfModelAlloy::~DramPerfModelAlloy()
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
DramPerfModelAlloy::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
	SubsecondTime model_delay = SubsecondTime::Zero();
	model_delay = m_dram_cache_cntlr->ProcessRequest(pkt_time, access_type, address);

	//printf("Alloy Model# pkt_time: %ld, address: %ld\n", pkt_time.getMS(), address);
	
	std::ofstream myfile;
	myfile.open ("DRAM_Access_orig_alloy.txt", std::ofstream::out | std::ofstream::app);
	myfile << "Alloy Model# pkt_time: " << pkt_time.getNS() << " address: " << address << std::endl;
	myfile.close();

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
