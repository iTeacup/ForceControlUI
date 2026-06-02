#pragma once
#include "CPSCfg.h"

class OptCfgParser
{
public:
	static OptCfgParser* Inst()
	{
		static OptCfgParser cfg;
		return &cfg;
	}

	bool LoadCfg();

public:
	// bus log cfg
	ST_BusLogCfg m_cpscfg = { 0 };
	// dev id
	int m_dev_id = -1;
	int m_push_freq_hz = 10;
	// opt cfg
	ST_OptServerCfg m_opt_cfg = { 0 };
private:
	OptCfgParser() = default;
};

// 全局静态变量
static OptCfgParser* g_cfg = OptCfgParser::Inst();
