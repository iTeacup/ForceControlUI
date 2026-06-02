#pragma once
#include "UIBaseWindow.h"
#include "UIImage.h"


class UIImageView :
    public UIBaseWindow
{
public:
	UIImageView(UIMainWindowBase* main_win, const char* title);
	virtual ~UIImageView();

	virtual void Draw();

	void ShowImageFromFile(const char* filename);
	void ShowImage(UIImageBasePtr image);

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_POPUP; }
protected:
	UIImageBasePtr m_image = nullptr;
	float m_image_zoom = 1.0f;
};

