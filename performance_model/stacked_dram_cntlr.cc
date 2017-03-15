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
	UInt32 v_i, b_i, p_i;
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

	n_banks = m_vault_size / m_bank_size;

	m_vaults_array = new VaultPerfModel*[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vaults_array[i] = new VaultPerfModel(vault_size, bank_size, row_size);
	}

	m_vremap_table = new VaultRemappingStructure(vaults_num);
	/* (REMAP_MAN) Remapping Managere, policy defined here*/
	m_remap_manager = new RemappingManager(this, 1);
	remapped = false;
	v_remap_times = b_remap_times = 0;

	tot_reads = tot_writes = tot_misses = 0;
	/* Initialize dram statistics*/
	tot_dram_reads = tot_dram_writes = tot_row_hits = 0;
	tot_act_t = tot_pre_t = tot_rd_t = tot_wr_t = SubsecondTime::Zero();
	
	/*
	   Initial DRAM simulator (ramulator)
	*/
	char *ram_config_file = "./ramulator/configs/HBM-config.cfg";
	m_dram_model = new DramModel(ram_config_file);
	first_req = true;
}

StackedDramPerfUnison::~StackedDramPerfUnison()
{
	/*
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
	*/

	// DEBUG
	log_file.close();

	delete [] m_vaults_array;
	delete m_vremap_table;
	delete m_dram_model;
	delete m_remap_manager;
}

SubsecondTime
StackedDramPerfUnison::getAccessLatency(
						SubsecondTime pkt_time, 
						UInt32 pkt_size, 
						UInt32 set_i, 
						DramCntlrInterface::access_t access_type)
{
	// bandwidth of DRAM, decides how many request we need
	UInt32 bandwidth = 128;
	UInt32 req_times = pkt_size / bandwidth;
	if (req_times < 1) {
		req_times = 1;
	}

	UInt32 vault_bit = floorLog2(n_vaults);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);

	UInt32 vault_i = (set_i >> row_bit) & ((1UL << vault_bit) - 1);
	UInt32 bank_i = set_i >> row_bit >> vault_bit; 
	UInt32 row_i = set_i & ((1UL << row_bit) - 1);

	/* Here we find out the remapping result*/

	bool valid_b = true, valid_v = true;
	//UInt32 remapVault = m_vremap_table->getVaultIdx(vault_i, &valid_v);
	//UInt32 remapBank = m_vremap_table->getBankIdx(vault_i, bank_i, &valid_b);
	// Handle an access in remapping
	//m_vremap_table->accessOnce(vault_i, bank_i, access_type, pkt_time);
	/* REMAP_MAN*/
	UInt32 remapVault = vault_i, remapBank = bank_i;
	bool valid_bit = m_remap_manager->getPhysicalIndex(&remapVault, &remapBank);

	//debug
#define LOG_OUTPUT
#ifdef LOG_OUTPUT
	log_file << "Set num: " << set_i 
			  << ", vault_i: " << vault_i
			  << ", bank_i: " << bank_i 
			  << ", row_i: " << row_i 
			  << "\n***remap vault and bank: "
			  << remapVault << " " << remapBank;
	if (remapVault != vault_i) {
		log_file << "-----------not original map\n";
	}
	log_file << "..." << valid_v << std::endl;

#endif
	/**/
	
	VaultPerfModel* vault = m_vaults_array[remapVault];

	SubsecondTime process_latency = SubsecondTime::Zero();

	//process_latency += vault->processRequest(pkt_time, access_type, remapBank, row_i);

	/*
	   (ramulator)
	   Here we can add Ramulator code
	   * generate DRAM operation based on cache access results
	   * get performance results
	   ----CONFIGURATION----
	   -Memory bandwidth
	   -Channel
	   -Bank
	   ----INPUT----
	   -DRAM address in HMC/HBM
	   -need iteration
	   ----OUTPUT----
	   -Latency
	   ----SOMETHING_ELSE----
	   Handle the fine grained stats in new model
	*/
	// We may need to tick memory during idle time
	UInt64 interval_ns = pkt_time.getNS() - last_req.getNS();
	interval_ns /= int(m_dram_model->tCK);
	/* Set the current time for ramulator
	TODO: More concrete 
	*/
	UInt32 tot_clks = 0, idle_clks = 0;
	last_req = pkt_time;
	//std::cout << "[IDLE] here we have interval_ns: " << interval_ns << std::endl;
	/*
TODO: Here we need to handle memory request with physical index
	   */
	/* (REMAP_MAN) Here we update statistics store unit*/
	m_remap_manager->accessRow(remapVault, remapBank, row_i, req_times);

	while (req_times > 0) {
		req_times --;

		bool stall = true;
		UInt32 clks = 0;
		if (access_type == DramCntlrInterface::READ) {
			stall = !m_dram_model->readRow(remapVault, remapBank, row_i, 0);
			m_dram_model->tickOnce();
			clks++;
			while (stall) {

				//std::cout << "here is a stall!!" << std::endl;

				stall = !m_dram_model->readRow(remapVault, remapBank, row_i, 0);
				m_dram_model->tickOnce();
				clks++;
			}
		} else {
			stall = !m_dram_model->writeRow(remapVault, remapBank, row_i, 0);
			m_dram_model->tickOnce();
			clks++;
			while (stall) {

				//std::cout << "here is a stall!!" << std::endl;

				stall = !m_dram_model->writeRow(remapVault, remapBank, row_i, 0);
				m_dram_model->tickOnce();
				clks++;
			}
		}
		clks += m_dram_model->getReadLatency(remapVault);

		UInt64 latency_ns = UInt64(m_dram_model->tCK) * clks;
		process_latency += SubsecondTime::NS(latency_ns);
		tot_clks += clks;
	}
#ifdef LOG_OUTPUT
	log_file << "--Latency clks: " << tot_clks
		     << ", idle clks: " << idle_clks
			 << ", read latency: " << m_dram_model->getReadLatency(remapVault)
			 << ", process_latency: " << process_latency.getNS()
			 << std::endl;
#endif
	return process_latency;
}

void
StackedDramPerfUnison::checkTemperature(UInt32 vault_i, UInt32 bank_i)
{
	std::cout << "check temperature (is useless)\n";

	/* balance vaults

	if (m_vremap_table->isTooHot(idx, bank_i)) {
		
		bool valid = false;
		UInt32 remap_id = m_vremap_table->getVaultIdx(idx, &valid);
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

	*/
}

void
StackedDramPerfUnison::checkStat()
{
	/* here we check stats of DRAM and swap (REMAP_MAN)*/
	std::cout << "[REMAP_MAN]Here we check DRAM stat to decide swap or not!" << std::endl;
	bool remap = false;
	
	if (m_remap_manager->policy == 1 || m_remap_manager->policy == 3) {
		for (UInt32 i = 0; i < n_vaults; i++) {
			bool rst = m_remap_manager->checkStat(i, 0, remap);
			if (rst) {
				v_remap_times ++;
			}
		}
	} else {
		for (UInt32 i = 0; i < n_vaults; i++) {
			for (UInt32 j = 0; j < n_banks; j++) {
				bool rst = m_remap_manager->checkStat(i, j, remap);
				if (rst) {
					b_remap_times ++;
				}
			}
		}
	}
	remapped = true;
}

void
StackedDramPerfUnison::checkDramValid(bool *valid_arr, UInt32* b_valid_arr, UInt32* b_migrated_arr)
{
	log_file << "[REMAP_CHECK] Here we start to check valid bit: ";

	for (int i = 0; i < n_vaults; i++) {
		UInt32 bank_valid = 0, bank_migrated = 0;
		//bool vault_valid = m_vremap_table->isValid(i, &bank_valid);
		bool vault_valid = true;
		for (int j = n_banks; j >= 0; j--) {
			UInt32 vault_i = i, bank_i = j;
			bool valid = m_remap_manager->getPhysicalIndex(&vault_i, &bank_i);
			bool migrated = m_remap_manager->checkMigrated(i, j);
			if (!valid) {
				bank_valid |= 1;

				log_file << i << "_" << j << "(" << vault_i << ")-invalid, " << bank_i;
			}
			if (migrated) {
				bank_migrated |= 1;

				log_file << i << "_" << j << "(" << vault_i << ")-migrated, " << bank_i;
			}
			bank_valid <<= 1;
		}
		if (vault_valid) {
			valid_arr[i] = true;
		} else {
			valid_arr[i] = false;
		}
		b_valid_arr[i] = bank_valid;
		b_migrated_arr[i] = bank_migrated;
	}
}

void
StackedDramPerfUnison::finishInvalidation()
{
	m_remap_manager->finishRemapping();
}

void
StackedDramPerfUnison::updateStats()
{
	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		vault->stats.reads = m_dram_model->getVaultRdReq(i);
		vault->stats.writes = m_dram_model->getVaultWrReq(i);
		vault->stats.row_hits = m_dram_model->getVaultRowHits(i);

		tot_dram_reads += vault->stats.reads;
		tot_dram_writes += vault->stats.writes;
		tot_row_hits += vault->stats.row_hits;

		for (UInt32 j = 0; j < vault->n_banks; j++) {
			BankPerfModel* bank = vault->m_banks_array[j];
			bank->stats.tACT = SubsecondTime::NS(m_dram_model->getBankActTime(i, j));
			bank->stats.tPRE = SubsecondTime::NS(m_dram_model->getBankActTime(i, j));
			bank->stats.tRD = SubsecondTime::NS(m_dram_model->getBankRdTime(i, j));
			bank->stats.tWR = SubsecondTime::NS(m_dram_model->getBankWrTime(i, j));

			bank->stats.reads = m_dram_model->getBankReads(i, j);
			bank->stats.writes = m_dram_model->getBankWrites(i, j);

			tot_act_t += bank->stats.tACT;
			tot_pre_t += bank->stats.tPRE;
			tot_rd_t += bank->stats.tRD;
			tot_wr_t += bank->stats.tWR;
		}
	}
}

void
StackedDramPerfUnison::clearCacheStats()
{
	tot_reads = tot_writes = tot_misses = 0;
}

void
StackedDramPerfUnison::updateTemperature(UInt32 v, UInt32 b, UInt32 temperature, UInt32 v_temp)
{
	m_remap_manager->updateTemperature(v, b, temperature, v_temp);
}

//-------------------------------------ALLOY-------------------------

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
