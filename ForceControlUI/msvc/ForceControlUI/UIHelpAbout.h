#pragma once
#include "UIBaseWindow.h"

class UIMainWindow;

class UIHelpAbout :
    public UIBaseWindow
{
public:
	UIHelpAbout(UIMainWindowBase* main_win, const char* title);
	virtual void Draw();

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_HELP; }
	virtual const char* GetShowShortCut() { return "F1"; }
protected:
	UIMainWindow* m_main_win = nullptr;
};

