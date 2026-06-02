#pragma once
#include "Config.h"
#include "UIMainWindow.h"

#ifdef UI_CFG_ENABLE_CPS
class CCPSAPI;
class CPSHandler;
#endif

class UIApplication
{
public:
	UIApplication();
	virtual ~UIApplication();

	void Run();
public:
	UIMainWindow* GetMainWindow() { return m_main_ui; }

#ifdef UI_CFG_ENABLE_CPS
public:
	CPSHandler* GetCPSHandler() { return m_handler; }
	CCPSAPI* GetCPSApi() { return m_cpsapi; }
	// calling from cps thread
	void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);
protected:
	void InitCPS();
protected:
	// CPS API
	CCPSAPI* m_cpsapi = nullptr;
	CPSHandler* m_handler = nullptr;
#endif
protected:
	void Exit();
protected:
	UIMainWindow* m_main_ui = nullptr;
};

// global APP instance
extern UIApplication g_app;
