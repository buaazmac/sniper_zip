#ifndef __CACHE_PAGE_INFO_H__
#define __CACHE_PAGE_INFO_H__

#include "fixed_types.h"
#include "cache_state.h"
#include "cache_base.h"

class CachePageInfo
{

	public:
		// Page size = 2KB, Block size = 64B
		// 1 Page has 32 blocks
		static const UInt32 BitsOfBlock = 5;

	private:
		IntPtr m_tag;
		CacheState::cstate_t m_cstate;
		UInt8 m_used;
	
	public:
		CachePageInfo();
		CachePageInfo(IntPtr tag,
				CacheState::cstate_t cstate);
		~CachePageInfo();

		static CachePageInfo* create();

		void invalidate(void);
		void clone(CachePageInfo* cache_page_info);

		bool isValid() const { return (m_tag != ((IntPtr) ~0)); }

		IntPtr getTag() const { return m_tag; }
		CacheState::cstate_t getCState() const { return m_cstate; }

		void setTag(IntPtr tag) { m_tag = tag; }
		void setCState(CacheState::cstate_t cstate) { m_cstate = cstate; }

		UInt8 getUsage() const { return m_used; }
		bool updateUsage(UInt32 offset, UInt32 size);
		bool updateUsage(UInt8 used);
};

#endif
