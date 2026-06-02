#pragma once
#include <CPSAPI/CPSAPI.h>
#include <CPSSRISensorDef.h>

class CSRICommManager;

class CSRICPSHandler : public CCPSEventHandler
{
public:
    CSRICPSHandler(CCPSAPI* cps_api, CSRICommManager* comm_manager);
    virtual ~CSRICPSHandler();

    // Connect event
    virtual void OnConnected() override;
    // Disconnect event
    virtual void OnDisconnected() override;

    // Message event
    virtual void OnMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len) override;

    void PushSensorData(ST_SRISensorData* sensor_data);
private:
    CCPSAPI* m_cps_api = nullptr;
    CSRICommManager* m_comm_manager = nullptr;
};
