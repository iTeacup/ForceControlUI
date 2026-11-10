#include "NokovCfgParser.h"
#include <CPSNokovDef.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
bool NokovCfgParser::LoadCfg(const std::filesystem::path& path)
{
    try
    {
        std::ifstream stream(path);
        if (!stream)
            throw std::runtime_error("Cannot open configuration");
        nlohmann::json cfg;
        stream >> cfg;
        serverIp = cfg.at("nokov").at("server_ip").get<std::string>();
        busIp = cfg.at("bus").at("ip").get<std::string>();
        busPort = cfg.at("bus").at("port").get<int>();
        auto capacity = cfg.at("queue_capacity").get<int>();
        if (cfg.at("dev_id").get<int>() != nokov::Device)
            throw std::runtime_error("NOKOV device ID must be 780");
        if (capacity < 1 || capacity > 65536 || busPort < 1 || busPort > 65535 || serverIp.empty() || busIp.empty())
            throw std::runtime_error("Invalid address, port or queue_capacity");
        queueCapacity = static_cast<size_t>(capacity);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << path << ": " << e.what() << std::endl;
        return false;
    }
}
