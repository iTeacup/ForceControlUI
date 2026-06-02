#pragma once
#include "CPSCfg.h"

class UICfgParser
{
public:
	static UICfgParser* Inst();

	bool LoadCfg();
	void SaveCfg();
public:
	// bus log cfg
	ST_BusLogCfg m_cpscfg = { 0 };
	// ui app id
	int m_app_id = -1;

	ST_MBCfg m_mb_cfg;
	ST_SerialCfg m_serial_cfg;
    ST_SerialCfg m_adc_serial_cfg;
	ST_SerialCfg m_com1_serial_cfg;
	ST_SerialCfg m_com2_serial_cfg;
private:
	UICfgParser() = default;
};

// 全局静态变量
static UICfgParser * g_cfg = UICfgParser::Inst();
