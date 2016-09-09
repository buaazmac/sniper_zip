#ifndef __CACHE_PAGE_INFO_H__
#define __CACHE_PAGE_INFO_H__

#include "fixed_types.h"
#include "cache_state.h"
#include "cache_base.h"

#include "core.h"

class CachePageInfo
{

	public:
		// Page size = 1984B, Block size = 64B
		// 1 Page has 31 blocks
		static const UInt32 BitsOfBlock = 5;

	private:
		IntPtr m_tag;

		UInt32 vbits_0;
		UInt32 vbits_1;
		UInt32 dbits_0;
		UInt32 dbits_1;

		UInt32 m_footprint;

		CacheState::cstate_t m_cstate;

		UInt8 m_used;
		UInt8 pc;
		UInt8 offset;
	
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
		UInt32 getValidBits() const { return vbits_0; }
		UInt32 getDirtyBits() const { return dbits_0; }
		UInt32 getFootprint() const { return m_footprint; }


		void setTag(IntPtr tag) { m_tag = tag; }
		void setCState(CacheState::cstate_t cstate) { m_cstate = cstate; }
		void setValidBits(UInt32 v) { vbits_0 = v; }
		void setDirtyBits(UInt32 d) { dbits_0 = d; }
		void setFootprint(UInt32 f) { m_footprint = f; }

		UInt8 getUsage() const { return m_used; }
		bool updateUsage(UInt32 offset, UInt32 size);
		bool updateUsage(UInt8 used);

		bool accessBlock(Core::mem_op_t, UInt32);

		void loadBlocks(UInt32 footprint);
};

#endif
