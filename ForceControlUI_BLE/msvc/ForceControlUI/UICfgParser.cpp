#include "UICfgParser.h"
#include <fstream>
#include "TimeCounter.h"
#include "Config.h"
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#define CFG_FILE "uicfg.json"
#define HELP_TREE_FILE "help/help.json"

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
		std::ifstream ifs(CFG_FILE);
		if (!ifs.is_open())
		{
			fprintf(stderr, "File %s does not exists!\n", CFG_FILE);
		}
		else
		{
			try
			{
				json cfg = json::parse(ifs);

                m_app_id = cfg["app_id"].template get<int>();
                strncpy(m_cpscfg.bus.ip, cfg["bus_server"]["ip"].template get<std::string>().c_str(), sizeof(m_cpscfg.bus.ip)-1);
                m_cpscfg.bus.port = cfg["bus_server"]["port"].template get<int>();
                strncpy(m_cpscfg.log.ip, cfg["log_server"]["ip"].template get<std::string>().c_str(), sizeof(m_cpscfg.log.ip)-1);
                m_cpscfg.log.port = cfg["log_server"]["port"].template get<int>();
				m_mb_cfg.baud_rate = GetBaudRateEnum(cfg["modbus"]["baud_rate"].template get<int>());
				m_mb_cfg.com_index = cfg["modbus"]["com_index"].template get<int>();

				m_serial_cfg.baud_rate = GetBaudRateEnum(cfg["serial"]["baud_rate"].template get<int>());
				m_serial_cfg.com_index = cfg["serial"]["com_index"].template get<int>();

				m_com1_serial_cfg.baud_rate = GetBaudRateEnum(cfg["serial"]["baud_rate"].template get<int>());
				m_com1_serial_cfg.com_index = cfg["serial"]["com_index"].template get<int>();

				m_com2_serial_cfg.baud_rate = GetBaudRateEnum(cfg["serial"]["baud_rate"].template get<int>());
				m_com2_serial_cfg.com_index = cfg["serial"]["com_index"].template get<int>();

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


void UICfgParser::SaveCfg()
{
	TimeCounter t;
	{
		std::ofstream ofs(CFG_FILE);
		if (!ofs.is_open())
		{
			fprintf(stderr, "File %s could not be opened for writing!\n", CFG_FILE);
		}
		else
		{
			try
			{
				json cfg;
                cfg["app_id"] = m_app_id;
                cfg["bus_server"]["ip"] = m_cpscfg.bus.ip;
                cfg["bus_server"]["port"] = m_cpscfg.bus.port;
                cfg["log_server"]["ip"] = m_cpscfg.log.ip;
                cfg["log_server"]["port"] = m_cpscfg.log.port;
				cfg["modbus"]["baud_rate"] = GetBaudRate(m_mb_cfg.baud_rate);
				cfg["modbus"]["com_index"] = m_mb_cfg.com_index;

				cfg["serial"]["baud_rate"] = GetBaudRate(m_serial_cfg.baud_rate);
				cfg["serial"]["com_index"] = m_serial_cfg.com_index;

				cfg["serial"]["baud_rate"] = GetBaudRate(m_com1_serial_cfg.baud_rate);
				cfg["serial"]["com_index"] = m_com1_serial_cfg.com_index;

				cfg["serial"]["baud_rate"] = GetBaudRate(m_com2_serial_cfg.baud_rate);
				cfg["serial"]["com_index"] = m_com2_serial_cfg.com_index;

                cfg["adc_serial"]["baud_rate"] = GetBaudRate(m_adc_serial_cfg.baud_rate);
                cfg["adc_serial"]["com_index"] = m_adc_serial_cfg.com_index;

				ofs << cfg.dump(4);
			}
			catch (const std::exception& e)
			{
				fprintf(stderr, "Saving %s exception: %s\n", CFG_FILE, e.what());
			}
			ofs.close();
		}
	}
	t.Tick(__FUNCTION__);
}
