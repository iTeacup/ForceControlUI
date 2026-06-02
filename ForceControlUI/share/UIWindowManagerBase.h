#pragma once
#include <memory>

class UIMainWindow;

class UIWindowManagerBase
{
public:
	UIWindowManagerBase(UIMainWindow* ui_main): m_ui_main(ui_main) {}

	// interface for initializing windows and menus
	virtual void Init() = 0;
	// interface for updating windows
	virtual void Draw() = 0;
	// interface for CPS messages
	virtual void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len) {}
	// interface for destroy windows
	virtual void Destroy() = 0;
public:
	UIMainWindow* GetUIMainWindow() { return m_ui_main; }
protected:
	UIMainWindow* m_ui_main = nullptr;
};

using UIWindowManagerBasePtr = std::shared_ptr<UIWindowManagerBase>;
