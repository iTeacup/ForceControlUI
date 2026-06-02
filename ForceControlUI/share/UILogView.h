#pragma once
#include "UIBaseWindow.h"

class UILogView :
    public UIBaseWindow
{
public:
	UILogView(UIMainWindowBase* main_win, const char* title);
	virtual void Draw();

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_VIEW; }
	virtual const char* GetShowShortCut() { return "Ctrl+L"; }
};

