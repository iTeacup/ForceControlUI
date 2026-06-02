#pragma once
#include "UIBaseWindow.h"
#include <vector>
#include <string>
#include "UIFontLoader.h"

class UIMainWindow;

struct UIFontFile
{
	UIFontFile(const std::string& name, const std::string& path) :font_name(name), font_file_path(path) {}
	std::string font_name;
	std::string font_file_path;
};


class UISysSettings :
    public UIBaseWindow
{
public:
	UISysSettings(UIMainWindowBase* main_win, const char* title);
	virtual void Draw();

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_SYS; }
	virtual const char* GetShowShortCut() { return "Ctrl+Shift+S"; }

protected:
	UIMainWindow* m_main_win = nullptr;
	bool m_vsync = true;

	std::vector<std::string> m_vec_theme_name;
	int m_style_idx = 0;
	char m_theme_name_join[1024] = { 0 };

	// 已知字体列表
	std::vector<UIFontFile> m_vec_font_files;
	// 当前选择的字体
	int m_cur_main_frame_font_idx = 0;
	float m_cur_main_font_size = 16.0f;
};
