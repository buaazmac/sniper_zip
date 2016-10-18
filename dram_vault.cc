#include "dram_vault.h"

VaultPerfModel::VaultPerfModel(UInt32 size, UInt32 bank_size, UInt32 row_size)
	: m_size(size),
	  m_bank_size(bank_size),
	  m_row_size(row_size)
{
	n_banks = m_size / bank_size;
	m_banks_array = new BankPerfModel*[n_banks];
	for (UInt32 i = 0; i < n_banks; i++) {
		m_banks_array[i] = new BankPerfModel(bank_size, row_size);
	}
}

VaultPerfModel::~VaultPerfModel()
{
	delete [] m_banks_array;
}

SubsecondTime
VaultPerfModel::processRequest(SubsecondTime cur_time, DramCntlrInterface::access_t access_type, UInt32 bank_i, UInt32 row_i)
{
	BankPerfModel* bank = m_banks_array[bank_i];
	int openned_row = bank->getOpennedRow();
	UInt32 op_row_u = openned_row;

	SubsecondTime process_latency = SubsecondTime::Zero();

	if (openned_row == -1) {

		process_latency += bank->processCommand(Command::ACT, row_i, cur_time);

	    openned_row = bank->getOpennedRow();
	} else if (op_row_u == row_i) {
	} else {
		process_latency += bank->processCommand(Command::ACT, row_i, cur_time + process_latency);
	}

	if (access_type == DramCntlrInterface::WRITE) {
		process_latency += bank->processCommand(Command::WR, row_i, cur_time + process_latency);
		process_latency += bank->processCommand(Command::PRE, openned_row, cur_time);
	} else {
		process_latency += bank->processCommand(Command::RD, row_i, cur_time + process_latency);
		process_latency += bank->processCommand(Command::PRE, openned_row, cur_time);
	}
	
	return process_latency;
}
