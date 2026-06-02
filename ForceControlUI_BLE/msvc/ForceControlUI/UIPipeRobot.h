#pragma once
#include "UIBaseWindow.h"
#include <CPSAPI/CPSAPI.h>
#include <CPSPipeRobotDef.h>

class UIPipeRobot : public UIBaseWindow
{
public:
    UIPipeRobot(UIMainWindowBase *main_win, const char *title);
    ~UIPipeRobot();

    virtual void Draw() override;
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char *GetShowShortCut() { return "Ctrl+7"; }

    virtual void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char *data, uint32_t msg_len) override;

protected:
    CCPSAPI *m_cpsapi = nullptr;
};
