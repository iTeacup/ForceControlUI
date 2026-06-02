#include "SRICPSHandler.h"
#include "sriRDSerial/sriCommManager.h"

CSRICPSHandler::CSRICPSHandler(CCPSAPI *cps_api, CSRICommManager *comm_manager) : m_comm_manager(comm_manager),
                                                                                  m_cps_api(cps_api)
{
}

CSRICPSHandler::~CSRICPSHandler()
{
}

void CSRICPSHandler::OnConnected()
{
    m_cps_api->RegisterDevice();
    CPS_INFO("CPS Connected.");
}

void CSRICPSHandler::OnDisconnected()
{
    CPS_INFO("CPS Disconnected.");
}

void CSRICPSHandler::OnMsg(uint32_t from_id, uint32_t msg_type, const char *data, uint32_t msg_len)
{
    switch (msg_type)
    {
    case MSG_REQ_SENSOR_RESET_ZERO:
        if (m_comm_manager)
        {
            m_comm_manager->ResetSensorZero();
            CPS_INFO("Sensor reset zero command received from %u.", from_id);
        }
        break;
    default:
        break;
    }
}

void CSRICPSHandler::PushSensorData(ST_SRISensorData *sensor_data)
{
    if (m_cps_api && sensor_data)
    {
        m_cps_api->SendDeviceMsg(-1, MSG_SRI_SENSOR_DATA, (const char *)sensor_data, sizeof(ST_SRISensorData));
    }
}
