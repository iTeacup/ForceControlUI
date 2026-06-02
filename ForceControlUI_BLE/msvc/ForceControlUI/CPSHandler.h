#pragma once
#include "Config.h"

#ifdef UI_CFG_ENABLE_CPS
#include <CPSAPI/CPSAPI.h>

class CPSHandler : public CCPSEventHandler
{
public:
	CPSHandler();
	~CPSHandler();

	// Connect event
	virtual void OnConnected();
	// Disconnect event
	virtual void OnDisconnected();
	// Device Online event
	virtual void OnDeviceOnline(uint32_t dev_id);
	// Device Offline event
	virtual void OnDeviceOffline(uint32_t dev_id);
	// Message event
	virtual void OnMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);
	// Get Connected status
	bool IsConnected() { return m_is_connected; }

protected:
	CCPSAPI* m_cpsapi = nullptr;
	bool m_is_connected = false;
};
#endif
