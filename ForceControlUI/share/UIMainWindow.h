#pragma once
#include <vector>
#include <map>
#include <string>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "UIBaseWindow.h"
#include "UILog.h"
#include "UIDef.h"
#include "UIWindowManagerBase.h"
#include "UIMenu.h"
#include "UIShortcut.h"

#ifdef UI_RENDER_ENGINE_OPENGL
#include "UIGLWindow.h"
#else
#include "UID3DWindow.h"
#endif

struct UIStyle
{
	ImGuiStyle	imstyle;
	ImVec4		bk_color1;
	ImVec4		bk_color2;
	int			fill_style; // Aspect_GradientFillMethod
};

class UIMainWindow : 
#ifdef UI_RENDER_ENGINE_OPENGL
	public UIGLWindow
#else
	public UID3DWindow
#endif
{
	friend UIBaseWindow;
public:
	UIMainWindow();
	virtual ~UIMainWindow();

	/* 初始化窗口系统 
	* title: window title
	* flags: UIMainWindowFlags
	*/
	void Init(const char* title, int flags);
	// 进入窗口循环
	void Loop();

	// 处理 cps message
	void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);
	// 销毁窗口（程序退出时调用）
	void Destroy();

	bool IsClosing() { return m_close; }
	bool IsInitialized() { return m_is_initialized; }

	void SetWindowManager(UIWindowManagerBasePtr win_mng) { m_win_mng = win_mng; }
	UIWindowManagerBasePtr GetWindowManager() { return m_win_mng; }
	ImGuiID GetRootDockSpaceID() { return m_dockspace_id; }

	void AddMainMenu(const UIMainMenu& menu);
	void AddMenuItem(UIMenuBasePtr menu);

	void AddShortCut(UIShortCutPtr sc);

	void UpdateStatusBar(const std::string& left_status , const std::string& right_status);
	void UpdateStatusLeft(const std::string& left_status) { m_left_status = left_status; }
	void UpdateStatusRight(const std::string& right_status) { m_right_status = right_status; }

	void ShowBannerLogoWindow(bool show) { m_show_logo_banner = show; }
	bool IsBannerLogoWindowVisible() { return m_show_logo_banner; }
public: // interface
	virtual float GetTitleBarHeight() { return m_title_bar_height; }
public: // public tool functions
	void ShowConfirmClose();
	void ShowHPGWarningClose();
public: // styles
	UIStyle* CurrentStyle();
	const std::string& CurrentStyleName();
	void SetUIStyle(const std::string& name);
	std::vector<std::string> GetStyleNameList();
	void SetUIDefaultStyleName(const std::string& name) { m_cur_style_name = name; }
	const std::string& GetUIDefaultStyleName() { return m_cur_style_name; }

	UIStyle GetDefaultStyle(const std::string& name);

	void UpdateScale(float xscale, float yscale);

	bool IsMainMenuVisible() { return m_show_main_menu; }
	void SetMainMenuVisible(bool visible) { m_show_main_menu = visible; }

	bool IsStatusBarVisible() { return m_show_status_bar; }
	void SetStatusBarVisible(bool visible) { m_show_status_bar = visible; }

	void SetUIMode(E_UI_MODE mode);
	void SetUIRefreshTimeout(double timeout);
	E_UI_MODE GetUIMode() { return m_ui_mode; }
	double GetUIRefreshTimeout() { return m_ui_refresh_timeout_sec; }
public:
	void* GetOldWndProc() { return m_old_wnd_proc; }
	void SetMouseCursor(ImGuiMouseCursor cursor) { m_cur_mouse_cursor = cursor; }
protected: // init functions
	void InitNativeWindow(const char* title, int flags);
	void InitImGui();
	void InitStyles();
protected: // functions in the loop
	// these three functions should be called in sequence in the main loop
	virtual void FrameEnd() override;
	virtual void FrameUpdate() override;

	void CreateRootWindow();
	void MakeAppMenu();
	void CreateStatusBar();

	void BeginNativeWindowFrame();
	void EndNativeWindowFrame();
	void DrawHeaderIcon();
	void DrawTitleBar();
protected: // window events
	//! Window close
	virtual void OnWindowClose() override;
	//! Window pos event
	virtual void OnWindowPos(int xpos, int ypos) override;
	//! Window resize event.
	virtual void OnWindowResize(int theWidth, int theHeight) override;
	//! Key callback
	virtual void OnKey(int key, int scancode, int action, int mods) override;
	//! Content scale callback
	virtual void OnContentScale(float xscale, float yscale) override;
private:
	UIWindowManagerBasePtr m_win_mng;
	// 是否显示菜单
	bool m_show_main_menu = true;
	// 是否显示状态栏
	bool m_show_status_bar = true;
	// 是否显示LogoBanner
	bool m_show_logo_banner = true;
	// 状态栏信息
	std::string m_left_status = "ready";
	std::string m_right_status = "";
	
	// UI 刷新模式
	E_UI_MODE m_ui_mode = E_UI_WAIT_TIMEOUT_MODE;
	double m_ui_refresh_timeout_sec = 1.0 / 30;;
	// 是否要关闭标识
	bool m_close = false;
	bool m_should_close = false;

	// 是否初始化完成
	bool m_is_initialized = false;
	// 菜单列表
	std::vector<UIMainMenu> m_main_menus;
	std::vector<UIMenuBasePtr> m_child_menus;
	// 函数快捷键列表
	std::vector<UIShortCutPtr> m_vec_shortcut;
	
	// 主题列表
	std::map<std::string, UIStyle > m_map_style;
	std::string m_cur_style_name = "Blue";

	// dock space id
	ImGuiID m_dockspace_id = 0;

	void* m_old_wnd_proc = NULL;
	float m_title_bar_height = 0.0f;
	ImGuiMouseCursor m_cur_mouse_cursor = ImGuiMouseCursor_Arrow;
};

// global APP instance
extern UIMainWindow *g_main_win;
