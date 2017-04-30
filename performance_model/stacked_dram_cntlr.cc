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

	//bool remap_config = Sim()->getCfg()->getBoolDefault("perf_model/dram_cache/remap", false);
	//std::cout << "Configuration: " << Sim()->getCfg()->getInt("perf_model/dram_cache/interval") << std::endl;

	n_banks = m_vault_size / m_bank_size;

	n_rows = m_bank_size / m_row_size;

	m_vaults_array = new VaultPerfModel*[n_vaults];
	for (UInt32 i = 0; i < n_vaults; i++) {
		m_vaults_array[i] = new VaultPerfModel(vault_size, bank_size, row_size);
	}

	//m_vremap_table = new VaultRemappingStructure(vaults_num);
	/* (REMAP_MAN) Remapping Managere, policy defined here*/
	m_remap_manager = new RemappingManager(this);

	/*
	Here we set the configuration for experiments using config file
	 */
	UInt32 c_max_remap_num = Sim()->getCfg()->getInt("perf_model/remap_config/max_remap_time");
	UInt32 c_row_access_threshold = Sim()->getCfg()->getInt("perf_model/remap_config/row_access_threshold");
	bool c_cross = Sim()->getCfg()->getBoolDefault("perf_model/remap_config/cross", true);
	bool c_invalid = Sim()->getCfg()->getBoolDefault("perf_model/remap_config/invalidation", true);
	bool c_mig = Sim()->getCfg()->getBoolDefault("perf_model/remap_config/migration", false);
	m_remap_manager->setRemapConfig(c_max_remap_num, c_row_access_threshold, c_cross, c_invalid, c_mig);


	remapped = false;
	enter_roi = false;

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
	// DEBUG
	log_file.close();

	delete [] m_vaults_array;
	//delete m_vremap_table;
	delete m_dram_model;
	delete m_remap_manager;
}

void 
StackedDramPerfUnison::splitSetNum(UInt32 set_i, UInt32* vault_i, UInt32* bank_i, UInt32* row_i)
{
	UInt32 vault_bit = floorLog2(n_vaults);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);

	int set_addr_map =  Sim()->getCfg()->getInt("perf_model/dram_cache/addr_map");

	if (set_addr_map == 1) { // v_b_r
		*vault_i = set_i >> row_bit >> bank_bit;
		*bank_i = (set_i >> row_bit) & ((1UL << bank_bit) - 1); 
		*row_i = set_i & ((1UL << row_bit) - 1);
	} else if (set_addr_map == 2) { // b_v_r
		*vault_i = (set_i >> row_bit) & ((1UL << vault_bit) - 1);
		*bank_i = set_i >> row_bit >> vault_bit; 
		*row_i = set_i & ((1UL << row_bit) - 1);
	} else if (set_addr_map == 3) { // r_b_v
		*vault_i = set_i & ((1UL << vault_bit) - 1);
		*bank_i = (set_i >> vault_bit) & ((1UL << bank_bit) - 1); 
		*row_i = set_i >> vault_bit >> bank_bit;
	} else {
		std::cout << "[Config Error] No such set address mapping configuration!\n";
	}

}
UInt32 
StackedDramPerfUnison::getSetNum(UInt32 vault_i, UInt32 bank_i, UInt32 row_i)
{
	UInt32 vault_bit = floorLog2(n_vaults);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);

	int set_addr_map =  Sim()->getCfg()->getInt("perf_model/dram_cache/addr_map");
	UInt32 set_i = 0;

	if (set_addr_map == 1) { // v_b_r
		set_i = vault_i << bank_bit << row_bit;
		set_i |= (bank_i << row_bit);
		set_i |= row_i;
	} else if (set_addr_map == 2) { // b_v_r
		set_i = bank_i << vault_bit << row_bit;
		set_i |= (vault_i << row_bit);
		set_i |= row_i;
	} else if (set_addr_map == 3) { // r_b_v
		set_i = row_i << vault_bit << bank_bit;
		set_i |= (bank_i << vault_bit);
		set_i |= vault_i;
	} else {
		std::cout << "[Config Error] No such set address mapping configuration!\n";
	}

	return set_i;
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

	/*
	UInt32 vault_bit = floorLog2(n_vaults);
	UInt32 bank_bit = floorLog2(m_vault_size / m_bank_size);
	UInt32 row_bit = floorLog2(m_bank_size / m_row_size);
	*/

	UInt32 vault_i = 0, bank_i = 0, row_i = 0;
	splitSetNum(set_i, &vault_i, &bank_i, &row_i);
	UInt32 ss = getSetNum(vault_i, bank_i, row_i);
	if (ss != set_i) {
		std::cout << "[ERROR] HooHoo, there is a problem with set number!\n";
	}
	/*
	UInt32 vault_i = (set_i >> row_bit) & ((1UL << vault_bit) - 1);
	UInt32 bank_i = set_i >> row_bit >> vault_bit; 
	UInt32 row_i = set_i & ((1UL << row_bit) - 1);
	*/

	/* Here we find out the remapping result*/

	bool valid_b = true, valid_v = true;
	//UInt32 remapVault = m_vremap_table->getVaultIdx(vault_i, &valid_v);
	//UInt32 remapBank = m_vremap_table->getBankIdx(vault_i, bank_i, &valid_b);
	// Handle an access in remapping
	//m_vremap_table->accessOnce(vault_i, bank_i, access_type, pkt_time);
	/* REMAP_MAN*/
	UInt32 remapVault = vault_i, remapBank = bank_i, remapRow = row_i;
	bool valid_bit = m_remap_manager->getPhysicalIndex(&remapVault, &remapBank, &remapRow);

	/**/
	
	VaultPerfModel* vault = m_vaults_array[remapVault];

	SubsecondTime process_latency = SubsecondTime::Zero();

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
	UInt64 clk_elasped = interval_ns / int(m_dram_model->tCK);
	if (pkt_time < last_req) {
		clk_elasped = 0;
	}
	if (clk_elasped > 1e6) {
		//std::cout << "[Ramulator] Too many ticks required!\n";
		clk_elasped = 1e6;
	}
	if (first_req) {
		first_req = false;
	} else {
		for (UInt64 i = 0; i < clk_elasped; i++) {
			m_dram_model->tickOnce();
		}
	}
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
	if (access_type != DramCntlrInterface::TRANS)
		m_remap_manager->accessRow(remapVault, remapBank, remapRow, req_times);

	/* STAT_DEBUG */
	if (access_type == DramCntlrInterface::READ) {
		tot_reads ++;
	} else {
		tot_writes ++;
	}

	while (req_times > 0) {
		req_times --;

		bool stall = true;
		UInt32 clks = 0;
		if (access_type == DramCntlrInterface::READ) {
			stall = !m_dram_model->readRow(remapVault, remapBank, remapRow, 0);
			m_dram_model->tickOnce();
			clks++;
			while (stall) {

				/* (REMAP_MAN) Here we update statistics store unit*/
				m_remap_manager->accessRow(remapVault, remapBank, remapRow, 1);

				stall = !m_dram_model->readRow(remapVault, remapBank, remapRow, 0);
				m_dram_model->tickOnce();
				clks++;
			}
		} else if (access_type == DramCntlrInterface::WRITE) {
			stall = !m_dram_model->writeRow(remapVault, remapBank, remapRow, 0);
			m_dram_model->tickOnce();
			clks++;
			while (stall) {

				/* (REMAP_MAN) Here we update statistics store unit*/
				m_remap_manager->accessRow(remapVault, remapBank, remapRow, 1);

				stall = !m_dram_model->writeRow(remapVault, remapBank, remapRow, 0);
				m_dram_model->tickOnce();
				clks++;
			}
		} else {
			// Transfer is a 2-clocks command
			clks += 2;
			m_dram_model->tickOnce();
			m_dram_model->tickOnce();
		}
		clks += m_dram_model->getReadLatency(remapVault);

		UInt64 latency_ns = UInt64(m_dram_model->tCK) * clks;
		process_latency += SubsecondTime::NS(latency_ns);

		tot_clks += clks;
	}
	return process_latency;
}

void
StackedDramPerfUnison::checkTemperature(UInt32 vault_i, UInt32 bank_i)
{
	std::cout << "check temperature (is useless)\n";
}

void
StackedDramPerfUnison::tryRemapping()
{
	UInt32 remap_times = m_remap_manager->tryRemapping(true);
	remapped = true;
}

void
StackedDramPerfUnison::checkStat()
{
	/* here we check stats of DRAM and swap (REMAP_MAN)*/
	//std::cout << "[REMAP_MAN]Here we check DRAM stat to decide swap or not!" << std::endl;
	/*
	bool remap = false;
	UInt32 remap_times = 0;
	
	for (UInt32 i = 0; i < n_vaults; i++) {
		for (UInt32 j = 0; j < n_banks; j++) {
			bool rst = m_remap_manager->checkStat(i, j, remap);
			if (rst) {
				b_remap_times ++;
				remap_times ++;
				if (remap_times > 10) break;
			}
		}
		if (remap_times > 10) break;
	}
	remapped = true;
	*/
}

bool
StackedDramPerfUnison::checkRowValid(UInt32 vault_i, UInt32 bank_i, UInt32 row_i)
{
	return m_remap_manager->checkValid(vault_i, bank_i, row_i);
}

bool 
StackedDramPerfUnison::checkRowMigrated(UInt32 vault_i, UInt32 bank_i, UInt32 row_i)
{
	return m_remap_manager->checkMigrated(vault_i, bank_i, row_i);
}

void
StackedDramPerfUnison::checkDramValid(bool *valid_arr, UInt32* b_valid_arr, UInt32* b_migrated_arr)
{
}

void
StackedDramPerfUnison::clearRemappingStat()
{
	m_remap_manager->finishRemapping();
	/* reset Remapping after each remap interval */
	m_remap_manager->resetStats();
	m_remap_manager->enableAllRemap();
}

void
StackedDramPerfUnison::updateStats()
{
	/* STAT_DEBUG*/
	//int memory_remaining_ticks = m_dram_model->interval_ticks;

	/* Tick Ramulator until queue empty*/
	/*
	for (int i = 0; i < memory_remaining_ticks; i++) {
		m_dram_model->tickOnce();
	}
	*/
	//m_dram_model->resetIntervalTick();
	/* Here we add code keeping remapping for a loner time
	* Reset remapping after every temperature change time
	*/
	//m_remap_manager->resetRemapping();

	for (UInt32 i = 0; i < n_vaults; i++) {
		VaultPerfModel* vault = m_vaults_array[i];
		vault->stats.reads = m_dram_model->getVaultRdReq(i);
		vault->stats.writes = m_dram_model->getVaultWrReq(i);
		vault->stats.row_hits = m_dram_model->getVaultRowHits(i);

		UInt32 serv_rd = m_dram_model->getServingRdReq(i);
		UInt32 serv_wr = m_dram_model->getServingWrReq(i);

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

			bank->stats.row_hits = m_dram_model->getBankRowHits(i, j);
			bank->stats.row_conflicts = m_dram_model->getBankRowConflicts(i, j);
			bank->stats.row_misses = m_dram_model->getBankRowMisses(i, j);

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

	/* Enable Remapping for all bank*/
	//m_remap_manager->enableAllRemap();
	
}

void
StackedDramPerfUnison::updateTemperature(UInt32 v, UInt32 b, UInt32 temperature, UInt32 v_temp)
{

	m_remap_manager->updateTemperature(v, b, temperature, v_temp);
	if (temperature >= 85) {
		m_vaults_array[v]->m_banks_array[b]->stats.hot = true;
		enter_roi = true;
		m_dram_model->setBankRef(v, b, true);
		
		//std::cout << " Set Higher Ref Freq in bank " << v << " " << b << std::endl;
	} else {
		m_vaults_array[v]->m_banks_array[b]->stats.hot = false;
		m_dram_model->setBankRef(v, b, false);
	}
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

	//m_vremap_table = new VaultRemappingStructure(vaults_num);
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
	//delete m_vremap_table;
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

	SubsecondTime process_latency = SubsecondTime::Zero();
	return process_latency;
}
