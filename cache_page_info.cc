#include "cache_page_info.h"
#include "log.h"

CachePageInfo::CachePageInfo()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
	m_used = 0;
	pc = 0;
	offset = 0;

	vbits_0 = ~0;
	vbits_1 = ~0;
	dbits_0 = 0;
	dbits_0 = 0;

	m_footprint = 0;
}

CachePageInfo::CachePageInfo(IntPtr tag, CacheState::cstate_t cstate)
	: m_tag(tag),
	  m_cstate(cstate),
	  m_used(0)
{}

CachePageInfo::~CachePageInfo()
{
}

void
CachePageInfo::invalidate()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
}

void
CachePageInfo::clone(CachePageInfo* cache_page_info)
{
	m_tag = cache_page_info->getTag();
	m_cstate = cache_page_info->getCState();
	m_used = cache_page_info->getUsage();

	
}

bool
CachePageInfo::updateUsage(UInt32 offset, UInt32 size)
{
	return true;
}

bool
CachePageInfo::updateUsage(UInt8 used)
{
	m_used = used;
	return true;
}

bool
CachePageInfo::accessBlock(Core::mem_op_t type, UInt32 block_num)
{
	bool v0 = (vbits_0 >> (block_num - 1)) & 1,
		 v1 = (vbits_1 >> (block_num - 1)) & 1,
		 d0 = (dbits_1 >> (block_num - 1)) & 1,
		 d1 = (dbits_1 >> (block_num - 1)) & 1;

	//set block valid anyway: load block if miss
	vbits_0 = vbits_0 | (1UL << (block_num - 1));
	//set block dirty if write operation
	if (type == Core::WRITE) {
		dbits_0 = dbits_0 | (1uL << (block_num - 1));
	}
	// set block footprint
	m_footprint = m_footprint | (1UL << (block_num - 1));
	if (v1 || d0 || d1) {}

	if (v0) return true;
	return false;
}

void
CachePageInfo::loadBlocks(UInt32 footprint)
{
	m_footprint = 0;
	vbits_0 = footprint;
}
