#pragma once
#include "CPSCfg.h"

class SRICfgParser
{
public:
    static SRICfgParser *Inst()
    {
        static SRICfgParser cfg;
        return &cfg;
    }
    bool LoadCfg();

public:
    // bus log cfg
    ST_BusLogCfg m_cpscfg = {0};
    // dev id
    int m_dev_id = -1;
    int m_push_freq_hz = 10;
    // sri sensor cfg
    ST_SRIServerCfg m_sri_cfg = {0};

private:
    SRICfgParser() = default;
};

// 全局静态变量
static SRICfgParser *g_cfg = SRICfgParser::Inst();
