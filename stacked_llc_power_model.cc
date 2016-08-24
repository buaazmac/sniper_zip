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
	m_set_info = new CacheSetInfoPageLRU(m_associativity);

	m_set = new CacheSetPageLRU*[m_set_num];	

	for (UInt32 i = 0; i < m_set_num; i++) {
		UInt32 offset = i >> 12;
		UInt32 vault_mask = (1ull << 5) - 1;
		m_set[i] = new CacheSetPageLRU(m_associativity, m_blocksize, m_set_info);

		m_set[i]->n_vault = offset & vault_mask;
		m_set[i]->n_level = (offset >> 5) & 3;
		
		//std::cout << m_set[i]->n_vault << ", " << m_set[i]->n_level << std::endl;
	}

	for (UInt32 i = 0; i < 32; i++) {
		for (UInt32 j = 0; j < 4; j++) {
			dram_stats[i][j].reads = 0;
			dram_stats[i][j].writes = 0;
		}
	}
}

StackedDramCacheCntlr::~StackedDramCacheCntlr()
{
	std::cout << "=-=deleting dram cache controller" << std::endl;

	std::ofstream myfile;
	myfile.open("DramCacheAccess.txt");

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

	delete m_set_info;
	delete [] m_set;
}

void
StackedDramCacheCntlr::ProcessRequest(Core::mem_op_t mem_op_type, IntPtr address)
{
	UInt32 set_n;
	IntPtr page_tag;
	if (SplitAddress(address, &set_n, &page_tag)) {
	} else {
		std::cout << "ERROR: process request from llc" << std::endl;
		return;
	}

	//std::cout << " Set num: " << set_n << ", Page tag: " << page_tag << std::endl;
	//std::cout << "\t\t:" << mem_op_type << std::endl;

	bool hit = m_set[set_n]->accessAttempt(page_tag);
	if (hit == false) {
		m_set[set_n]->reads ++;
		UInt32 index = m_set[set_n]->getReplacementIndex(NULL);
		m_set[set_n]->updateReplacementIndexTag(index, page_tag);
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
StackedDramCacheCntlr::SplitAddress(IntPtr address, UInt32 *set_n, IntPtr *page_tag)
{
	UInt64 addr = address >> floorLog2(m_pagesize);
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
