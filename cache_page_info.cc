#include "cache_page_info.h"
#include "log.h"

CachePageInfo::CachePageInfo()
{
	m_tag = ~0;
	m_cstate = CacheState::INVALID;
	m_used = 0;
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
