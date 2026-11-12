#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "UICfgParser.h"
#include <fstream>
#include "TimeCounter.h"
#include "Config.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <Windows.h>
#endif

using json = nlohmann::json;

#define CFG_FILE "uicfg.json"
#define HELP_TREE_FILE "help/help.json"

namespace
{
std::filesystem::path ConfigPath()
{
#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (size > 0 && size < executable.size())
    {
        executable.resize(size);
        return std::filesystem::path(executable).parent_path() / CFG_FILE;
    }
#endif
    return CFG_FILE;
}
}

UICfgParser* UICfgParser::Inst()
{
	static UICfgParser p;
	return &p;
}


bool UICfgParser::LoadCfg()
{
	TimeCounter t;
	// parse app cfg
	{
		std::ifstream ifs(ConfigPath());
		if (!ifs.is_open())
		{
			fprintf(stderr, "File %s does not exists!\n", CFG_FILE);
		}
		else
		{
			try
			{
				json cfg = json::parse(ifs);
                m_ur_model_path.clear();
                const auto teleop = cfg.find("ur_teleop");
                if (teleop != cfg.end() && teleop->is_object())
                {
                    const auto path = teleop->find("model_path");
                    if (path != teleop->end() && path->is_string()) m_ur_model_path = path->get<std::string>();
                }

                m_app_id = cfg["app_id"].template get<int>();
                strncpy(m_cpscfg.bus.ip, cfg["bus_server"]["ip"].template get<std::string>().c_str(), sizeof(m_cpscfg.bus.ip)-1);
                m_cpscfg.bus.port = cfg["bus_server"]["port"].template get<int>();
                strncpy(m_cpscfg.log.ip, cfg["log_server"]["ip"].template get<std::string>().c_str(), sizeof(m_cpscfg.log.ip)-1);
                m_cpscfg.log.port = cfg["log_server"]["port"].template get<int>();
				m_mb_cfg.baud_rate = GetBaudRateEnum(cfg["modbus"]["baud_rate"].template get<int>());
				m_mb_cfg.com_index = cfg["modbus"]["com_index"].template get<int>();

				m_serial_cfg.baud_rate = GetBaudRateEnum(cfg["serial"]["baud_rate"].template get<int>());
				m_serial_cfg.com_index = cfg["serial"]["com_index"].template get<int>();

				const json& com1_serial_cfg = cfg.contains("com1_serial") ? cfg["com1_serial"] : cfg["serial"];
				m_com1_serial_cfg.baud_rate = GetBaudRateEnum(com1_serial_cfg["baud_rate"].template get<int>());
				m_com1_serial_cfg.com_index = com1_serial_cfg["com_index"].template get<int>();

				const json& com2_serial_cfg = cfg.contains("com2_serial") ? cfg["com2_serial"] : cfg["serial"];
				m_com2_serial_cfg.baud_rate = GetBaudRateEnum(com2_serial_cfg["baud_rate"].template get<int>());
				m_com2_serial_cfg.com_index = com2_serial_cfg["com_index"].template get<int>();

                m_adc_serial_cfg.baud_rate = GetBaudRateEnum(cfg["adc_serial"]["baud_rate"].template get<int>());
                m_adc_serial_cfg.com_index = cfg["adc_serial"]["com_index"].template get<int>();
			}
			catch (const std::exception& e)
			{
				fprintf(stderr, "Parsing %s exception: %s\n", CFG_FILE, e.what());
			}
		}
		ifs.close();
	}
	t.Tick(__FUNCTION__);
	return true;
}


bool UICfgParser::SaveCfg()
{
	TimeCounter t;
    bool saved = false;
	{
		std::ofstream ofs(ConfigPath());
		if (!ofs.is_open())
		{
			fprintf(stderr, "File %s could not be opened for writing!\n", CFG_FILE);
		}
		else
		{
			try
			{
				json cfg;
                cfg["ur_teleop"]["model_path"] = m_ur_model_path;
                cfg["app_id"] = m_app_id;
                cfg["bus_server"]["ip"] = m_cpscfg.bus.ip;
                cfg["bus_server"]["port"] = m_cpscfg.bus.port;
                cfg["log_server"]["ip"] = m_cpscfg.log.ip;
                cfg["log_server"]["port"] = m_cpscfg.log.port;
				cfg["modbus"]["baud_rate"] = GetBaudRate(m_mb_cfg.baud_rate);
				cfg["modbus"]["com_index"] = m_mb_cfg.com_index;

				cfg["serial"]["baud_rate"] = GetBaudRate(m_serial_cfg.baud_rate);
				cfg["serial"]["com_index"] = m_serial_cfg.com_index;

				cfg["com1_serial"]["baud_rate"] = GetBaudRate(m_com1_serial_cfg.baud_rate);
				cfg["com1_serial"]["com_index"] = m_com1_serial_cfg.com_index;

				cfg["com2_serial"]["baud_rate"] = GetBaudRate(m_com2_serial_cfg.baud_rate);
				cfg["com2_serial"]["com_index"] = m_com2_serial_cfg.com_index;

                cfg["adc_serial"]["baud_rate"] = GetBaudRate(m_adc_serial_cfg.baud_rate);
                cfg["adc_serial"]["com_index"] = m_adc_serial_cfg.com_index;

				ofs << cfg.dump(4);
                saved = static_cast<bool>(ofs);
			}
			catch (const std::exception& e)
			{
				fprintf(stderr, "Saving %s exception: %s\n", CFG_FILE, e.what());
			}
			ofs.close();
            saved = saved && !ofs.fail();
		}
	}
	t.Tick(__FUNCTION__);
    return saved;
}
