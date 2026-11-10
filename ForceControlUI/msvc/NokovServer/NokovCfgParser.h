#pragma once
#include <filesystem>
#include <string>
struct NokovCfgParser
{
    std::string serverIp, busIp;
    int busPort = 4000;
    size_t queueCapacity = 2048;
    bool LoadCfg(const std::filesystem::path& path);
};
