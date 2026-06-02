#pragma once
#include <string>
#include <vector>
#include "UIBaseWindow.h"
#include "ModbusProxy.h"
#include "ModbusDef.h"

class UIConnectionSettings :
    public UIBaseWindow
{
public:
	UIConnectionSettings(UIMainWindowBase* main_win, const char* title);
	~UIConnectionSettings();

	virtual void Draw();

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_TOOL; }
	virtual const char* GetShowShortCut() { return "Ctrl+Shift+1"; }

	ModbusProxy* GetModbusProxy() { return &m_modbus; }
protected:
	EMBConnType m_con_type = E_RTU;
	int m_slave_id_index = -1;
    std::vector<int> m_slave_ids;

	std::vector<std::string> m_vec_coms;

	// modbus proxy
	ModbusProxy m_modbus;
	bool m_show_holdregs = false;
    system_settings_t m_sys_settings = {0};
	version_info_t m_version = { 0 };
	system_status_t m_status = { 0 };

	bool m_connect_failed_warning = false;
	std::string m_warning_msg = "";
};

