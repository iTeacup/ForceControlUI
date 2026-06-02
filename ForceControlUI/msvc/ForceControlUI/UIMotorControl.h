#pragma once
#include "UIBaseWindow.h"
#include "ModbusProxy.h"
#include "common_datadef.h"

class UIMotorControl : public UIBaseWindow
{
public:
    UIMotorControl(UIMainWindowBase *main_win, const char *title);
    ~UIMotorControl();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char *GetShowShortCut() { return "Ctrl+1"; }

protected:
    ModbusProxy *m_modbus = nullptr;

    lamotor_cmds_t m_lamotor_cmds = {0};
    homotor_cmds_t m_homotor_cmds = {0};

    lamotor_info_t m_lamotor_info[LA_MOTOR_NUM] = {0};
    homotor_info_t m_homotor_info[HO_MOTOR_NUM] = {0};
};
