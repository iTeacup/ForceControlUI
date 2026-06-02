#include "CPSHandler.h"
#ifdef UI_CFG_ENABLE_CPS
#include "UIApplication.h"
#include "UILog.h"
#include <CPSAPI/CPSDef.h>

#ifdef _WIN32
#pragma comment(lib, "cpsapi.lib")
#endif

CPSHandler::CPSHandler()
{
	m_cpsapi = g_app.GetCPSApi();
}

CPSHandler::~CPSHandler()
{
}

void CPSHandler::OnConnected()
{
	m_is_connected = true;
    m_cpsapi->SubscribeDevice(OPT_SERVER_DEV_ID);
    m_cpsapi->SubscribeDevice(SENSOR_SERVER_DEV_ID);
    m_cpsapi->SubscribeDevice(HYPERSEN_SENSOR_DEV_ID);
    m_cpsapi->SubscribeDevice(FINRAY_CALC_SERVER_DEV_ID);
    m_cpsapi->SubscribeDevice(PIPE_ROBOT_DEV_ID);
    m_cpsapi->SubscribeDevice(SRI_SENSOR_DEV_ID);

	m_cpsapi->SubscribeDevice(OTHER_SENSORS_DEV_ID);
	UI_INFO(u8"总线已连接！");
}

void CPSHandler::OnDisconnected()
{
	m_is_connected = false;
	UI_INFO(u8"总线已断开！");
}

void CPSHandler::OnDeviceOnline(uint32_t dev_id)
{
	UI_INFO(u8"设备<%d>已在线！", dev_id);
}

void CPSHandler::OnDeviceOffline(uint32_t dev_id)
{
	UI_INFO(u8"设备<%d>已离线！", dev_id);
}

void CPSHandler::OnMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len)
{
	// forward message to view msg handler
	g_app.OnCPSMsg(from_id, msg_type, data, msg_len);
}

#endif