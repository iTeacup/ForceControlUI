#pragma once
#include <CPSAPI/CPSAPI.h>
#include <CPSNokovDef.h>
#include <atomic>
class Handler : public CCPSEventHandler
{
public:
    CCPSAPI* api = nullptr;
    std::atomic_bool desired{false}, online{false};
    void OnConnected() override
    {
        online = true;
        api->RegisterDevice();
    }
    void OnDisconnected() override { online = false; }
    void OnMsg(uint32_t, uint32_t type, const char* data, uint32_t size) override
    {
        if (!data || type != nokov::ControlMessage || size != sizeof(nokov::Control))
            return;
        nokov::Control command;
        memcpy(&command, data, sizeof(command));
        if (command.version == nokov::Version)
            desired = command.connect != 0;
    }
};
