#ifndef CACHE_SET_PAGE_LRU_H
#define CACHE_SET_PAGE_LRU_H

#include "cache_set.h"
#include "cache_page_info.h"

#include "core.h"

#include <iostream>

class CacheSetInfoPageLRU : public CacheSetInfo
{
	public:
		CacheSetInfoPageLRU(UInt32 associativity);
		virtual ~CacheSetInfoPageLRU();
		void increment(UInt32 index) {
			++m_access[index];
		}

	private:
		const UInt32 m_associativity;
		UInt64* m_access;
};

class CacheSetPageLRU : public CacheSet
{
	public:
		CacheSetPageLRU(UInt32 associativity, UInt32 blocksize, 
				CacheSetInfoPageLRU* set_info);
		virtual ~CacheSetPageLRU();

		virtual UInt32 getReplacementIndex(CacheCntlr*);
		virtual void updateReplacementIndex(UInt32);
		void updateReplacementIndexTag(UInt32 index, IntPtr tag, IntPtr pc, IntPtr offset, UInt32 footprint);

		void updateUsedInfo(IntPtr tag);

		UInt8 accessAttempt(Core::mem_op_t type, IntPtr tag, IntPtr offset);

		int reads;
		int writes;
		//int acts;

		UInt32 n_vault;
		UInt32 n_bank;
		UInt32 n_level;

		CachePageInfo** m_cache_page_info_array;

	protected:
		UInt8* m_lru_bits;
		CacheSetInfoPageLRU* m_set_info;
		void moveToMRU(UInt32 accessed_index);
};

#endif
