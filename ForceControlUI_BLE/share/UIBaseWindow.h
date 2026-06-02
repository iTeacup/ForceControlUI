#pragma once
#include <stdint.h>
#include "Config.h"

class UIMainWindowBase;

// 窗口所属菜单类别
enum class EUIMenuCategory
{
	E_UI_CAT_APP,
	E_UI_CAT_EDIT,
	E_UI_CAT_SYS,
	E_UI_CAT_VIEW,
	E_UI_CAT_HELP,
	E_UI_CAT_SERVICE,
	E_UI_CAT_TOOL,
	E_UI_CAT_LAYOUT,
	E_UI_CAT_SUB_MENU, // 子菜单窗口
	E_UI_CAT_POPUP // 弹出式界面，不在菜单显示打开按钮
};
/*
* 所有UI界面的基类
*/
class UIBaseWindow
{
public:
	UIBaseWindow(UIMainWindowBase* main_win, const char* title);
	virtual ~UIBaseWindow() {}
public: // interface
	//virtual void Init() {}
	virtual void Draw() {}
	virtual EUIMenuCategory GetWinMenuCategory() = 0;
	virtual const char* GetShowShortCut() { return ""; }
	virtual void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len) {}
public: // helper func
	bool* GetShowVar() { return &m_show; }
	char* GetWinTitle() { return m_win_title; }
	void Show() { m_show = true; }
	void Hide() { m_show = false; }
	void ToggleVisible() { m_show = !m_show; }
	bool Visible() { return m_show; }
public: // static func
	static void DrawHelpTip(const char* help);
	static void DrawHelpTipForLastItem(const char* help);
protected:
	bool m_show = false;
	char m_win_title[64];
	UIMainWindowBase* m_main_win;
};
