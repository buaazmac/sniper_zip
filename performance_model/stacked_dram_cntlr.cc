#include "stacked_dram_cntlr.h"

#include <iostream>
#include <fstream>

StackedDramPerfMem::StackedDramPerfMem(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size)
	: n_vaults(vaults_num),
	  m_vault_size(vault_size),
	  m_bank_size(bank_size),
	  m_row_size(row_size)
{
	/*DEBUG*/
	log_file.open ("mem_log.txt");

	m_vaults_array = new VaultPerfModel*[n_vaults];

	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vaults_array[i] = new VaultPerfModel(vault_size, bank_size, row_size);
	}
}

StackedDramPerfMem::~StackedDramPerfMem()
{
	std::ofstream myfile;
	myfile.open ("StackedDramMem.txt", std::ofstream::out | std::ofstream::app);
	myfile << "Simulation start" << std::endl;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		for (UInt32 j = 0; j < vault->n_banks; j++) {
			BankPerfModel* bank = vault->m_banks_array[j];
			myfile << "dram_" << i << "_" << j << " " 
					<< bank->stats.tACT << " " 
					<< bank->stats.tPRE << " " 
					<< bank->stats.tRD << " " 
					<< bank->stats.tWR << std::endl;
		}
	}
	//myfile << "End." << std::endl;
	myfile.close();

	// DEBUG
	log_file.close();

	delete [] m_vaults_array;

}

SubsecondTime
StackedDramPerfMem::getAccessLatency(
					SubsecondTime pkt_time, 
					UInt32 pkt_size, 
					IntPtr address, 
					DramCntlrInterface::access_t access_type)
{

	UInt32 offset = address & ((1 << OFF_BIT) -1 );
	UInt64 addr = address >> OFF_BIT;
	int v_i, b_i, p_i;
	v_i = addr & ((1 << VAULT_BIT) - 1);
	addr = addr >> VAULT_BIT;
	b_i = addr & ((1 << BANK_BIT) - 1);
	addr = addr >> BANK_BIT;
	p_i = addr & ((1 << PART_BIT) - 1);
	addr = addr >> PART_BIT;

	addr = (addr << OFF_BIT) | offset;

	VaultPerfModel* vault = m_vaults_array[v_i];
	UInt32 bank_i = p_i * 2 + b_i;
	UInt32 row_i = (addr >> floorLog2(m_row_size)) & ((1UL << 11) -1 );

	SubsecondTime process_time = SubsecondTime::Zero();

	//debug
	log_file  << ", bank_i: " << bank_i 
			  << ", row_i: " << row_i << std::endl;
	
	process_time += vault->processRequest(pkt_time, access_type, bank_i, row_i);

	return process_time;
}

StackedDramPerfCache::StackedDramPerfCache(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size)
	: n_vaults(vaults_num),
	  m_vault_size(vault_size),
	  m_bank_size(bank_size),
	  m_row_size(row_size)
{
	/*DEBUG*/
	log_file.open ("cache_log.txt");

	m_vaults_array = new VaultPerfModel*[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vaults_array[i] = new VaultPerfModel(vault_size, bank_size, row_size);
	}
}

StackedDramPerfCache::~StackedDramPerfCache()
{
	std::ofstream myfile;
	myfile.open("StackedDramCache.txt");

	myfile << "Simulation start" << std::endl;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		for (UInt32 j = 0; j < vault->n_banks; j++) {
			BankPerfModel* bank = vault->m_banks_array[j];
			myfile << "dram_" << i << "_" << j << " " 
					<< bank->stats.tACT << " " 
					<< bank->stats.tPRE << " " 
					<< bank->stats.tRD << " " 
					<< bank->stats.tWR << std::endl;
		}
	}
	myfile.close();

	// DEBUG
	log_file.close();

	delete [] m_vaults_array;
}

SubsecondTime
StackedDramPerfCache::getAccessLatency(
						SubsecondTime pkt_time, 
						UInt32 pkt_size, 
						IntPtr set_i, 
						DramCntlrInterface::access_t access_type)
{
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);

	log_file << "m_vault_size: " << m_vault_size
			 << "m_bank_size: " << m_bank_size
			  << ", bank_bit: " << bank_bit 
			  << ", row_bit: " << row_bit << std::endl;

	UInt32 vault_i = set_i >> bank_bit >> row_bit;
	UInt32 bank_i = (set_i >> row_bit) & ((1UL << bank_bit) - 1); 
	UInt32 row_i = set_i & ((1UL << row_bit) - 1);
	//debug
	log_file << "Set num: " << set_i 
			  << ", vault_i: " << vault_i
			  << ", bank_i: " << bank_i 
			  << ", row_i: " << row_i << std::endl;
	
	VaultPerfModel* vault = m_vaults_array[vault_i];

	SubsecondTime process_latency = SubsecondTime::Zero();

	process_latency += vault->processRequest(pkt_time, access_type, bank_i, row_i);
	return process_latency;
}
