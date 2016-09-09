#include "simulator.h"
#include "config.h"
#include "config.hpp"
#include "stats.h"
#include "stacked_llc_power_model.h"


/*
   Master Controller for Stacked DRAM Cache
   STACKED_LLC
   */

StackedDramCacheCntlr::StackedDramCacheCntlr(
		UInt32 set_num, UInt32 associativity, UInt32 blocksize, UInt32 pagesize)
	: m_set_num(set_num),
	  m_associativity(associativity),
	  m_blocksize(blocksize),
	  m_pagesize(pagesize)
{
	std::cout << "Total set: " << set_num << std::endl;
	m_set_info = new CacheSetInfoPageLRU(m_associativity);

	m_set = new CacheSetPageLRU*[m_set_num];	

	for (UInt32 i = 0; i < 31; i++) {
		FHT[i] = 0;
	}

	for (UInt32 i = 0; i < m_set_num; i++) {
		UInt32 offset = i >> 12;
		UInt32 vault_mask = (1ull << 5) - 1;
		m_set[i] = new CacheSetPageLRU(m_associativity, m_blocksize, m_set_info);

		m_set[i]->n_vault = offset & vault_mask;
		m_set[i]->n_bank = (i >> 11) & 1;
		m_set[i]->n_level = (offset >> 5) & 3;
		
		//std::cout << m_set[i]->n_vault << ", " << m_set[i]->n_level << std::endl;
	}

	for (UInt32 i = 0; i < 32; i++) {
		for (UInt32 j = 0; j < 4; j++) {
			dram_stats[i][j].reads = 0;
			dram_stats[i][j].writes = 0;
		}
	}
	log_file.open("SetAccess.txt");
}

StackedDramCacheCntlr::~StackedDramCacheCntlr()
{
	std::cout << "=-=deleting dram cache controller" << std::endl;

	std::ofstream myfile;
	myfile.open("DramCacheAccess.txt");

	myfile << "Simulationg start" << std::endl;

	for (UInt32 i = 0; i < m_set_num; i++) {
		UInt32 vn = m_set[i]->n_vault, ln = m_set[i]->n_level;
		dram_stats[vn][ln].reads += m_set[i]->reads;
		dram_stats[vn][ln].writes += m_set[i]->writes;
	}

	for (UInt32 i = 0; i < 32; i++) {
		for (UInt32 j = 0; j < 4; j++) {
			myfile << "dram_cache_" << i << "_" << j << " " << dram_stats[i][j].reads << " " << dram_stats[i][j].writes << std::endl;
		}
	}

	myfile.close();

	log_file.close();

  /*

	std::ofstream myfile2;
	myfile2.open("SetAccess.txt");

	myfile2 << "Simulationg start" << std::endl;

	for (UInt32 i = 0; i < m_set_num; i++) {
		myfile2 << "setnum_" << i << ": " <<m_set[i]->reads  << ", " << m_set[i]->writes << std::endl;
		}

	myfile2.close();
 //  */

	delete m_set_info;
	delete [] m_set;
}

void
StackedDramCacheCntlr::ProcessRequest(Core::mem_op_t mem_op_type, IntPtr address)
{
	UInt32 set_n;
	IntPtr page_tag;
	IntPtr page_offset;
	UInt64 pc = 0;
	UInt32 footprint = 0;

	if (SplitAddress(address, &set_n, &page_tag, &page_offset)) {
	} else {
		std::cout << "ERROR: process request from llc" << std::endl;
		return;
	}

	// DEBUG_LOG
	log_file << "Origin address: " << address << ", Set num: " << set_n << ", Page tag: " << page_tag << std::endl;
	//std::cout << "\t\t:" << mem_op_type << std::endl;

	UInt8 block_num = page_offset / m_blocksize;
	footprint = FHT[block_num];

	UInt8 hit = m_set[set_n]->accessAttempt(mem_op_type, page_tag, page_offset);
	if (hit == 0) { // page is not in cache
		m_set[set_n]->reads ++;
		UInt32 index = m_set[set_n]->getReplacementIndex(NULL);
		// page eviction
		/*
		   1. alter page tag
		   2. load footprint blocks: set vbits & dbits
		   3. set pc & offset bits
		   4. update footprint history table
		   */

		// page eviction & load new page
		m_set[set_n]->updateReplacementIndexTag(index, page_tag, pc, page_offset, footprint);

		// update footprint history table
		UInt32 new_footprint = m_set[set_n]->m_cache_page_info_array[index]->getFootprint();
		FHT[block_num] = new_footprint;
	} else if (hit == 1) { // page is in cache, but block is not
		// we need to load block from memory
		// we also need to update page's footprint
	}
	if (mem_op_type == Core::READ) {
		//read data from the page
		m_set[set_n]->reads ++;
	} else if (mem_op_type == Core::READ_EX) {
		//read data from the page
		m_set[set_n]->reads ++;
	} else if (mem_op_type == Core::WRITE) {
		//write data to the page
		m_set[set_n]->writes ++;
	} else {
	}
}

bool
StackedDramCacheCntlr::SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *page_tag, IntPtr *page_offset)
{
	*page_offset = address % m_pagesize;
	UInt64 addr = address / m_pagesize;
	*set_n = addr & ((1UL << floorLog2(m_set_num)) - 1);
	*page_tag = addr >> floorLog2(m_set_num);
	return true;
}


//----------------------------STACKED_LLC: StackedDramCacheCntrl-----------

StackedLlcPowerModel::StackedLlcPowerModel(UInt32 size, UInt32 sets, UInt32 assoc):
	cache_size(size), 
	num_sets(sets),
	associativity(assoc)
{
	num_layers  = PART_NUM;

	for (int i = 0; i < VAULT_NUM; i++) {
		vaults[i] = new StackedDramVault();
	}

	std::ofstream myfile;
	myfile.open("LLC_Access.txt");
	myfile << "Simulation start" << std::endl;
	myfile.close();
}

StackedLlcPowerModel::~StackedLlcPowerModel() {
	std::ofstream myfile;
	myfile.open("LLC_Access.txt", std::ofstream::out | std::ofstream::app);
	//output statistics
	myfile.close();

	for (int i = 0; i < VAULT_NUM; i++) {
		delete vaults[i];
	}
}
