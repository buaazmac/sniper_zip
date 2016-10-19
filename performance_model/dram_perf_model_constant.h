#ifndef __DRAM_PERF_MODEL_CONSTANT_H__
#define __DRAM_PERF_MODEL_CONSTANT_H__

#include "dram_perf_model.h"
#include "queue_model.h"
#include "fixed_types.h"
#include "subsecond_time.h"
#include "dram_cntlr_interface.h"

#include "stacked_dram_cntlr.h"

class SrcEntry {
	public:
		UInt32 m_tag;
		UInt32 count;
		UInt32 m_access[6];
		UInt32 start_address;
		UInt32 swap_count;
		SrcEntry(UInt32 seg_num) : m_tag(0), count(0) {
			for (UInt32 i = 0; i < 6; i++) {
				m_access[i] = 0;
			}
			start_address = (1 << 20) * seg_num;
			// Every 32 access swap segments
			swap_count = 20;
		}

		int accessEntry(UInt32 n) {
			count++;
			m_access[n]++;
			bool hit = (n == m_tag);
			bool swap = false;
			if (count == swap_count) {
				count = 0;
				UInt32 max = 0, idx = 0;
				for (int i = 0; i < 6; i++) {
					if (m_access[i] > max) {
						max = m_access[i];
						idx = i;
					}
					m_access[i] = 0;
				}
				if (idx != m_tag) {
					m_tag = idx;
					swap = true;
				}
			}
			if (hit) {
				if (swap) {
					return 3;
				} else {
					return 2;
				}
			} else {
				if (swap) {
					return 1;
				} else {
					return 0;
				}
			}
		}
};

class DramPerfModelConstant : public DramPerfModel
{
   private:
      QueueModel* m_queue_model;
      SubsecondTime m_dram_access_cost;
      ComponentBandwidth m_dram_bandwidth;

      SubsecondTime m_total_queueing_delay;
      SubsecondTime m_total_access_latency;


	  UInt32 fast_m_size; // MB
	  UInt32 tot_m_size; // MB
	  UInt32 seg_size; // MB
	  UInt32 seg_num;
	  SrcEntry **SRC; // recording segments in fast entry
      //	StackedDramVault* vaults[VAULT_NUM];

	  StackedDramPerfMem* m_dram_perf_model;

	  UInt32 tot_access, swap_times;


   public:
      DramPerfModelConstant(core_id_t core_id,
            UInt32 cache_block_size);

      ~DramPerfModelConstant();

      SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf);
};

#endif /* __DRAM_PERF_MODEL_CONSTANT_H__ */
