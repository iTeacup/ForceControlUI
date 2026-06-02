#include "OptCfgParser.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#define OPT_CFG_FILE	"optcfg.json"

bool OptCfgParser::LoadCfg()
{
    std::ifstream ifs(OPT_CFG_FILE);
    if (!ifs.is_open())
    {
        // 文件打开失败
        fprintf(stderr, "Failed to open config file: %s\n", OPT_CFG_FILE);
        return false;
    }

    try
    {
        json jcfg;
        ifs >> jcfg;

        // 解析 bus log cfg
        {
            strncpy(m_cpscfg.bus.ip, jcfg["bus"]["ip"].get<std::string>().c_str(), sizeof(m_cpscfg.bus.ip) - 1);
            m_cpscfg.bus.port = jcfg["bus"]["port"].get<int>();
            strncpy(m_cpscfg.log.ip, jcfg["log"]["ip"].get<std::string>().c_str(), sizeof(m_cpscfg.log.ip) - 1);
            m_cpscfg.log.port = jcfg["log"]["port"].get<int>();
            m_dev_id = jcfg["dev_id"].get<int>();
            m_push_freq_hz = jcfg["push_freq_hz"].get<int>();
        }
        // 解析 opt cfg
        {
            strncpy(m_opt_cfg.server_ip, jcfg["opt"]["server_ip"].get<std::string>().c_str(), sizeof(m_opt_cfg.server_ip) - 1);
            strncpy(m_opt_cfg.local_ip, jcfg["opt"]["local_ip"].get<std::string>().c_str(), sizeof(m_opt_cfg.local_ip) - 1);
        }
    }
    catch (const std::exception &e)
    {
        // 解析异常
        fprintf(stderr, "Failed to parse config file: %s, exception: %s\n", OPT_CFG_FILE, e.what());
        ifs.close();
        return false;
    }
    ifs.close();
    fprintf(stdout, "Config file %s loaded successfully.\n", OPT_CFG_FILE);
    return true;
}
