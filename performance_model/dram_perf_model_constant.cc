#include "dram_perf_model_constant.h"
#include "simulator.h"
#include "config.h"
#include "config.hpp"
#include "stats.h"
#include "shmem_perf.h"

#include <iostream>
#include <fstream>

DramPerfModelConstant::DramPerfModelConstant(core_id_t core_id,
      UInt32 cache_block_size):
   DramPerfModel(core_id, cache_block_size),
   m_queue_model(NULL),
   m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")), // Convert bytes to bits
   m_total_queueing_delay(SubsecondTime::Zero()),
   m_total_access_latency(SubsecondTime::Zero())
{
/*
	Initializing information of hardware solution for part of memory
   */
	fast_m_size = 4 * 1024;
	tot_m_size = 20 * 1024;
	seg_size = 1;
	seg_num = fast_m_size / seg_size;
	SRC = new SrcEntry*[seg_num];

	for (UInt32 i = 0; i < seg_num; i++) {
		SRC[i] = new SrcEntry(i);
	}
	
	m_dram_perf_model = new StackedDramPerfMem(32, 128*1024, 16*1024, 8);

	Sim()->getStatsManager()->init_stacked_dram_mem(m_dram_perf_model);
	
	tot_access = swap_times = 0;

	/*-----------------------------*/

   m_dram_access_cost = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/latency"))); // Operate in fs for higher precision before converting to uint64_t/SubsecondTime

   if (Sim()->getCfg()->getBool("perf_model/dram/queue_model/enabled"))
   {
      m_queue_model = QueueModel::create("dram-queue", core_id, Sim()->getCfg()->getString("perf_model/dram/queue_model/type"),
                                         m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
   }

   registerStatsMetric("dram", core_id, "total-access-latency", &m_total_access_latency);
   registerStatsMetric("dram", core_id, "total-queueing-delay", &m_total_queueing_delay);
	
}

DramPerfModelConstant::~DramPerfModelConstant()
{


   if (m_queue_model)
   {
     delete m_queue_model;
      m_queue_model = NULL;
   }

   std::cout << "[EXTRA OUTPUT] Total Memory Request: " << tot_access << ", Swap Times: " << swap_times << std::endl;
   
   delete [] SRC;
   delete m_dram_perf_model;
}

SubsecondTime
DramPerfModelConstant::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
   
	//std::cout << "Constant Model: " << "pkt_time: " << pkt_time << "address: " << address <<  std::endl;
	//printf("Constant Model# pkt_time: %ld, address: %ld\n", pkt_time.getMS(), address);


	UInt64 seg_tag = address >> 20; // seg: 1 MB
	UInt32 src_n = seg_tag % seg_num;
	UInt32 nth_seg = (seg_tag / seg_num) % 6;

	/*
	bool src_miss = false;
	if (nth_seg != SRC[src_n]->m_tag) {
		src_miss = true;
	} 
	*/

  	int rst = SRC[src_n]->accessEntry(nth_seg);

	/*DEBUG*/
	/*
	std::ofstream myfile;
	myfile.open ("DRAM_Access_orig_2.txt", std::ofstream::out | std::ofstream::app);
	myfile << "Constant Model# pkt_time: " << pkt_time.getNS() << " address: " << address << " --- " << (address >> OFF_BIT) << std::endl;
	myfile.close();
	*/

    SubsecondTime model_delay = SubsecondTime::Zero();
	SubsecondTime load_delay = SubsecondTime::Zero();


   // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bits per clock cycle'
   if ((!m_enabled) ||
         (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
   {
      return SubsecondTime::Zero();
   }

   //Here we calculate time
	// If swapping happens 
   tot_access ++;
	if (rst == 1 || rst == 3) {
		swap_times++;
		// One segment read and write happend in stacked dram
		UInt32 cur_addr = SRC[src_n]->start_address;
		for (int i = 0; i < (1 << 7); i++) {
			model_delay += m_dram_perf_model->getAccessLatency(pkt_time, pkt_size, cur_addr, DramCntlrInterface::READ);
			model_delay += m_dram_perf_model->getAccessLatency(pkt_time, pkt_size, cur_addr, DramCntlrInterface::WRITE);
			cur_addr += (1 << 13);
		}
		// Load 1 MB from slow memory
		load_delay += m_dram_bandwidth.getRoundedLatency(1 << 23);
	}

   if (rst == 0 || rst == 1) { // not hit
	   // Read data from slow memory
		load_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size); 
   }
   
	model_delay += m_dram_perf_model->getAccessLatency(pkt_time, pkt_size, address, access_type);

   SubsecondTime access_latency = model_delay + load_delay;


   perf->updateTime(pkt_time);
   perf->updateTime(pkt_time + model_delay);
   perf->updateTime(pkt_time + access_latency);

   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += access_latency;

   return access_latency;
}
