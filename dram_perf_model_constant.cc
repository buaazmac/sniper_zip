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
		SRC[i] = new SrcEntry();
	}
	


   m_dram_access_cost = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/latency"))); // Operate in fs for higher precision before converting to uint64_t/SubsecondTime

   if (Sim()->getCfg()->getBool("perf_model/dram/queue_model/enabled"))
   {
      m_queue_model = QueueModel::create("dram-queue", core_id, Sim()->getCfg()->getString("perf_model/dram/queue_model/type"),
                                         m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
   }

   registerStatsMetric("dram", core_id, "total-access-latency", &m_total_access_latency);
   registerStatsMetric("dram", core_id, "total-queueing-delay", &m_total_queueing_delay);
	
	std::ofstream myfile;
	myfile.open ("DRAM_Access.txt");
	myfile << "Simulation start" << std::endl;
	myfile.close();

	for (int i = 0; i < VAULT_NUM; i++) {
		vaults[i] = new StackedDramVault();
	}
}

DramPerfModelConstant::~DramPerfModelConstant()
{

	std::ofstream myfile;
	myfile.open ("DRAM_Access.txt", std::ofstream::out | std::ofstream::app);

	for (int i = 0; i < VAULT_NUM; i++) {
		for (int j = 0; j < PART_NUM; j++) {
			myfile << "dram_" << i << "_" << j << " " << vaults[i]->parts[j]->reads << " " << vaults[i]->parts[j]->writes << std::endl;
		}
	}

	//myfile << "End." << std::endl;

	myfile.close();

   if (m_queue_model)
   {
     delete m_queue_model;
      m_queue_model = NULL;
   }
   
   for (int i = 0; i < VAULT_NUM; i++) {
	   delete vaults[i];
   }

   delete [] SRC;
}

SubsecondTime
DramPerfModelConstant::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
   
	//std::cout << "Constant Model: " << "pkt_time: " << pkt_time << "address: " << address <<  std::endl;
	//printf("Constant Model# pkt_time: %ld, address: %ld\n", pkt_time.getMS(), address);

	/*
	std::ofstream myfile;
	myfile.open ("DRAM_Access.txt", std::ofstream::out | std::ofstream::app);
	myfile << "Constant Model# pkt_time: " << pkt_time.getNS() << " address: " << address << std::endl;
	*/

	UInt64 seg_tag = address >> 20; // seg: 1 MB
	UInt32 src_n = seg_tag % seg_num;
	UInt32 nth_seg = (seg_tag / seg_num) % 6;
<<<<<<< HEAD

	bool src_miss = false;
	if (nth_seg != SRC[src_n]->m_tag) {
		src_miss = true;
	}

  	int rst = SRC[src_n]->accessEntry(nth_seg);
=======

	bool src_miss = false;
	if (nth_seg != SRC[src_n]->m_tag) {
		src_miss = true;
	}

  	bool swap = SRC[src_n]->accessEntry(nth_seg);
>>>>>>> 836c84e6a7ff8170f36181a0cb7498834a3da90d

	UInt64 addr = address >> OFF_BIT;
	int v_i, b_i, p_i;
	v_i = addr & ((1 << VAULT_BIT) - 1);
	addr = addr >> VAULT_BIT;
	b_i = addr & ((1 << BANK_BIT) - 1);
	addr = addr >> BANK_BIT;
	p_i = addr & ((1 << PART_BIT) - 1);

<<<<<<< HEAD
	//power model does no matter with hit
	if (rst == 1 || rst == 3) {
		vaults[v_i]->writes++;
		vaults[v_i]->reads++;
		vaults[v_i]->parts[p_i]->reads++;
		vaults[v_i]->parts[p_i]->writes++;
=======
	if (swap) {
		vaults[v_i]->writes++;
		vaults[v_i]->reads++;
>>>>>>> 836c84e6a7ff8170f36181a0cb7498834a3da90d
	}

	vaults[v_i]->reads++;
	vaults[v_i]->parts[p_i]->reads++;
	//StackedDramBank* bb = vaults[v_i]->parts[p_i]->banks[b_i];
	//bb->AccessOnce(0);

	/*
	myfile << "----access--v_" << v_i << "--b_" << b_i << "--p_" << p_i << ": " << bb->GetPower() << ", " << vaults[v_i]->parts[p_i]->reads << ", " << vaults[v_i]->reads << std::endl;

	myfile.close();
	*/

   // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bits per clock cycle'
   if ((!m_enabled) ||
         (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
   {
      return SubsecondTime::Zero();
   }

   
   SubsecondTime processing_time = m_dram_bandwidth.getRoundedLatency(8 * pkt_size); // bytes to bits

   //Here we calculate time
   SubsecondTime model_delay = SubsecondTime::Zero();
   if (rst == 0) {
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
   } else if (rst == 1) {
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
	   //now we assume swap need 1 read + 1 write = 3 read
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
   } else if (rst == 2) {
   } else {
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
	   model_delay += m_dram_bandwidth.getRoundedLatency(8 * pkt_size);
   }
   

   // Compute Queue Delay
   SubsecondTime queue_delay;
   if (m_queue_model)
   {
      queue_delay = m_queue_model->computeQueueDelay(pkt_time, processing_time, requester);
   }
   else
   {
      queue_delay = SubsecondTime::Zero();
   }

   SubsecondTime access_latency = queue_delay + processing_time + m_dram_access_cost + model_delay;


   perf->updateTime(pkt_time);
   perf->updateTime(pkt_time + queue_delay, ShmemPerf::DRAM_QUEUE);
   perf->updateTime(pkt_time + queue_delay + processing_time, ShmemPerf::DRAM_BUS);
   perf->updateTime(pkt_time + queue_delay + processing_time + m_dram_access_cost, ShmemPerf::DRAM_DEVICE);

   // Update Memory Counters
   m_num_accesses ++;
   m_total_access_latency += access_latency;
   m_total_queueing_delay += queue_delay;

   return access_latency;
}
