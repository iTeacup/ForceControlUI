#pragma once
#include "UIBaseWindow.h"
#include <string>
#include <vector>
#include "UIIconsFontDef.h"

class UIIconsFontView :
    public UIBaseWindow
{
public:
	UIIconsFontView(UIMainWindowBase* main_win, const char* title);
	virtual void Draw();

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_SUB_MENU; }

protected:
	void FilterIcons();
protected:
	std::vector<SIconFont> m_icons;
	std::vector<SIconFont> m_icons_filtered;
	std::string m_filter = "";
};

