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
	myfile.open ("StackedDramMem.txt");
	myfile << "Simulation start" << std::endl;

	SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
	tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

	UInt32 tot_access, tot_row_hits;

	tot_access = tot_row_hits = 0;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		for (UInt32 j = 0; j < vault->n_banks; j++) {
			BankPerfModel* bank = vault->m_banks_array[j];
			myfile << "dram_" << i << "_" << j << " " 
					<< bank->stats.tACT << " " 
					<< bank->stats.tPRE << " " 
					<< bank->stats.tRD << " " 
					<< bank->stats.tWR << " "
					<< bank->stats.reads << " "
					<< bank->stats.writes << " "
					<< bank->stats.row_hits << std::endl;

			tot_ACT += bank->stats.tACT;
			tot_PRE += bank->stats.tPRE;
			tot_RD += bank->stats.tRD;
			tot_WR += bank->stats.tWR;

			tot_access += bank->stats.reads + bank->stats.writes;
			tot_row_hits += bank->stats.row_hits;
		}
	}
	float row_hit_rate = 0;
	if (tot_access != 0) 
		row_hit_rate = (float)tot_row_hits / (float)tot_access;

	myfile << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
	myfile << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
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
	// we have 2k rows each bank
	UInt32 row_i = (addr >> floorLog2(m_row_size)) & ((1UL << 11) -1 );

	SubsecondTime process_time = SubsecondTime::Zero();

	//debug
#ifdef LOG_OUTPUT
	log_file  << "address: " << address
			  << ", vault_i: " << v_i 
			  << ", bank_i: " << bank_i 
			  << ", row_i: " << row_i << std::endl;
#endif
	
	process_time += vault->processRequest(pkt_time, access_type, bank_i, row_i);

	return process_time;
}

StackedDramPerfUnison::StackedDramPerfUnison(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size)
	: n_vaults(vaults_num),
	  m_vault_size(vault_size),
	  m_bank_size(bank_size),
	  m_row_size(row_size)
{
	/*DEBUG*/
	log_file.open ("unison_log.txt");

	m_vaults_array = new VaultPerfModel*[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vaults_array[i] = new VaultPerfModel(vault_size, bank_size, row_size);
	}

	m_vremap_table = new VaultRemappingStructure(vaults_num);
	
}

StackedDramPerfUnison::~StackedDramPerfUnison()
{
	std::ofstream myfile;
	myfile.open("StackedDramUnison.txt");

	myfile << "Simulation start" << std::endl;

	SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
	tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

	UInt32 tot_access, tot_row_hits;

	tot_access = tot_row_hits = 0;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		for (UInt32 j = 0; j < vault->n_banks; j++) {
			BankPerfModel* bank = vault->m_banks_array[j];
			myfile << "dram_" << i << "_" << j << " " 
					<< bank->stats.tACT << " " 
					<< bank->stats.tPRE << " " 
					<< bank->stats.tRD << " " 
					<< bank->stats.tWR << " "
					<< bank->stats.reads << " "
					<< bank->stats.writes << " "
					<< bank->stats.row_hits << std::endl;
			tot_ACT += bank->stats.tACT;
			tot_PRE += bank->stats.tPRE;
			tot_RD += bank->stats.tRD;
			tot_WR += bank->stats.tWR;

			tot_access += bank->stats.reads + bank->stats.writes;
			tot_row_hits += bank->stats.row_hits;
		}
	}
	float row_hit_rate = 0;
	if (tot_access != 0) 
		row_hit_rate = (float)tot_row_hits / (float)tot_access;

	myfile << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
	myfile << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
	myfile.close();

	// DEBUG
	log_file.close();

	delete [] m_vaults_array;
	delete m_vremap_table;
}

SubsecondTime
StackedDramPerfUnison::getAccessLatency(
						SubsecondTime pkt_time, 
						UInt32 pkt_size, 
						UInt32 set_i, 
						DramCntlrInterface::access_t access_type)
{
	UInt32 vault_bit = floorLog2(n_vaults);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);

	UInt32 vault_i = (set_i >> row_bit) & ((1UL << vault_bit) - 1);
	UInt32 bank_i = set_i >> row_bit >> vault_bit; 
	UInt32 row_i = set_i & ((1UL << row_bit) - 1);

	/* Here we find out the remapping result*/

	bool valid_b = true, valid_v = true;
	UInt32 remapVault = m_vremap_table->getVaultIdx(vault_i, &valid_v);
	UInt32 remapBank = m_vremap_table->getBankIdx(vault_i, bank_i, &valid_b);
	// Handle an access in remapping
	m_vremap_table->accessOnce(vault_i, bank_i, access_type);

	//debug
#ifdef LOG_OUTPUT
	log_file << "Set num: " << set_i 
			  << ", vault_i: " << vault_i
			  << ", bank_i: " << bank_i 
			  << ", row_i: " << row_i 
			  << "\n***remap vault: "
			  << remapVault;
	if (remapVault != vault_i) {
		log_file << "-----------not original map\n";
	}
	log_file << "..." << valid_v << std::endl;

#endif
	/**/
	
	VaultPerfModel* vault = m_vaults_array[remapVault];

	SubsecondTime process_latency = SubsecondTime::Zero();

	process_latency += vault->processRequest(pkt_time, access_type, remapBank, row_i);

	return process_latency;
}

void
StackedDramPerfUnison::checkTemperature(UInt32 idx, UInt32 bank_i)
{
	/* balance vaults*/

	if (m_vremap_table->isTooHot(idx, bank_i)) {
		
		bool valid = false;
		UInt32 remap_id = m_vremap_table->getVaultIdx(idx, &valid);
#define LOG_OUTPUT
#ifdef LOG_OUTPUT
		log_file << "@Vault_" << idx << " is too hot!\n";
		log_file << "@original map: " 
				 << remap_id
				 << "  valid: " << valid
				 << std::endl;
#endif

		m_vremap_table->balanceVaults(idx);

#ifdef LOG_OUTPUT
		remap_id = m_vremap_table->getVaultIdx(idx, &valid);
		log_file << "@now map: " 
				 << remap_id 
				 << "  valid: " << valid
				 << std::endl;
#endif
	}

	/**/
}

void
StackedDramPerfUnison::checkDramValid(bool *valid_arr, int* banks)
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		int bank_valid;
		bool vault_valid = m_vremap_table->isValid(i, &bank_valid);
		banks[i] = bank_valid;
		if (vault_valid) {
			valid_arr[i] = true;
		} else {
			valid_arr[i] = false;
		}
	}
}

void
StackedDramPerfUnison::finishInvalidation()
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vremap_table->setValid(i);
	}
}

StackedDramPerfAlloy::StackedDramPerfAlloy(UInt32 vaults_num, UInt32 vault_size, UInt32 bank_size, UInt32 row_size)
	: n_vaults(vaults_num),
	  m_vault_size(vault_size),
	  m_bank_size(bank_size),
	  m_row_size(row_size)
{
	/*DEBUG*/
	log_file.open ("alloy_log.txt");

	m_vaults_array = new VaultPerfModel*[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vaults_array[i] = new VaultPerfModel(vault_size, bank_size, row_size);
	}

	m_vremap_table = new VaultRemappingStructure(vaults_num);
}

StackedDramPerfAlloy::~StackedDramPerfAlloy()
{
	std::ofstream myfile;
	myfile.open("StackedDramAlloy.txt");

	myfile << "Simulation start" << std::endl;

	SubsecondTime tot_ACT, tot_PRE, tot_RD, tot_WR;
	tot_ACT = tot_PRE = tot_RD = tot_WR = SubsecondTime::Zero();

	UInt32 tot_access, tot_row_hits;

	tot_access = tot_row_hits = 0;

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		for (UInt32 j = 0; j < vault->n_banks; j++) {
			BankPerfModel* bank = vault->m_banks_array[j];
			myfile << "dram_" << i << "_" << j << " " 
					<< bank->stats.tACT << " " 
					<< bank->stats.tPRE << " " 
					<< bank->stats.tRD << " " 
					<< bank->stats.tWR << " "
					<< bank->stats.reads << " "
					<< bank->stats.writes << " "
					<< bank->stats.row_hits << std::endl;
			tot_ACT += bank->stats.tACT;
			tot_PRE += bank->stats.tPRE;
			tot_RD += bank->stats.tRD;
			tot_WR += bank->stats.tWR;

			tot_access += bank->stats.reads + bank->stats.writes;
			tot_row_hits += bank->stats.row_hits;
		}
	}
	float row_hit_rate = 0;
	if (tot_access != 0) 
		row_hit_rate = (float)tot_row_hits / (float)tot_access;

	myfile << "Total time: " << tot_ACT << ", " << tot_PRE << ", " << tot_RD << ", " << tot_WR << std::endl;
	myfile << "Total access: "  << tot_access << ", Row Hit Rate: " << row_hit_rate << std::endl;
	myfile.close();

	// DEBUG
	log_file.close();

	delete [] m_vaults_array;
	delete m_vremap_table;
}

SubsecondTime
StackedDramPerfAlloy::getAccessLatency(
						SubsecondTime pkt_time, 
						UInt32 pkt_size, 
						UInt32 set_i, 
						DramCntlrInterface::access_t access_type)
{
	UInt32 vault_bit = floorLog2(n_vaults);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);

#ifdef LOG_OUTPUT
	log_file << "m_vault_size: " << m_vault_size
			 << "m_bank_size: " << m_bank_size
			  << ", bank_bit: " << bank_bit 
			  << ", row_bit: " << row_bit << std::endl;
#endif

	UInt32 vault_i = (set_i >> row_bit) & ((1UL << vault_bit) - 1);
	UInt32 bank_i = set_i >> row_bit >> vault_bit; 
	UInt32 row_i = set_i & ((1UL << row_bit) - 1);

	/* Here we find out the remapping result*/

	bool valid = true;
	UInt32 remapVault = m_vremap_table->getVaultIdx(vault_i, &valid);

	//debug
#define LOG_OUTPUT
#ifdef LOG_OUTPUT
	log_file << "Set num: " << set_i 
			  << ", vault_i: " << vault_i
			  << ", bank_i: " << bank_i 
			  << ", row_i: " << row_i 
			  << "\n***remap vault: "
			  << remapVault << std::endl;
	if (remapVault != vault_i) {
		log_file << "-----------not original map\n";
	}
#endif
	/**/
	
	VaultPerfModel* vault = m_vaults_array[remapVault];

	SubsecondTime process_latency = SubsecondTime::Zero();

	process_latency += vault->processRequest(pkt_time, access_type, bank_i, row_i);
	return process_latency;
}
