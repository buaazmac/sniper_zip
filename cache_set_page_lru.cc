#include "cache_set_page_lru.h"
#include "log.h"
#include "stats.h"

// Implement Stacked-DRAM page-based cache set with LRU replacement


CacheSetInfoPageLRU::CacheSetInfoPageLRU(UInt32 associativity)
	: m_associativity(associativity)
{
	m_access = new UInt64[m_associativity];
	for (UInt32 i = 0; i < m_associativity; i++) {
		m_access[i] = 0;
	}
}

CacheSetInfoPageLRU::~CacheSetInfoPageLRU()
{
	delete [] m_access;
}

CacheSetPageLRU::CacheSetPageLRU(
		UInt32 associativity, UInt32 blocksize, CacheSetInfoPageLRU* set_info) 
		: CacheSet(CacheBase::STACKED_DRAM_CACHE, associativity, blocksize),
		  reads(0),
		  writes(0),
		  m_set_info(set_info)
{
	m_lru_bits = new UInt8[m_associativity];
	for (UInt32 i = 0; i < m_associativity; i++) {
		m_lru_bits[i] = i;
	}

	m_cache_page_info_array = new CachePageInfo*[m_associativity];
	for (UInt32 i = 0; i < m_associativity; i++) {
		m_cache_page_info_array[i] = new CachePageInfo();
	}
}

CacheSetPageLRU::~CacheSetPageLRU()
{
	for (UInt32 i = 0; i < m_associativity; i++) {
		delete m_cache_page_info_array[i];
	}
	delete [] m_lru_bits;
	delete [] m_cache_page_info_array;
}

UInt32
CacheSetPageLRU::getReplacementIndex(CacheCntlr* tlr)
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
CacheSetPageLRU::updateUsedInfo(IntPtr tag)
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

void
CacheSetPageLRU::updateReplacementIndex(UInt32 index)
{
}

void
CacheSetPageLRU::updateReplacementIndexTag(UInt32 index, IntPtr tag)
{
	//here we simulate the replacement process
	m_cache_page_info_array[index]->setTag(tag);
	
	//increase the stats
	reads++;
	writes++;

	updateUsedInfo(tag);
}

bool
CacheSetPageLRU::accessAttempt(IntPtr tag)
{
	//here we simulate the looking up process
	reads++;
	for (UInt32 i = 0; i < m_associativity; i++) {
		
		if (m_cache_page_info_array[i]->getTag() == tag) {
			return true;
		}
	}
	return false;
}

