#pragma once
#include "UIBaseWindow.h"
#include <thread>
#include <mutex>
#include <vector>
#include "URManager.h"

class UIURobot : public UIBaseWindow
{
public:
    UIURobot(UIMainWindowBase *main_win, const char *title);
    ~UIURobot();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char *GetShowShortCut() { return "Ctrl+5"; }

    URManager *GetURManager() { return &m_ur; }
protected:
    char m_hostname[64] = {0}; // 机器人IP地址

    URManager m_ur;
    
    ST_URDataFrame m_ur_data = {0};
    ST_RobotiqGripperDataFrame m_gripper_data = {0};
    float m_ur_move_speed = 0.5f; // UR MoveJ/MoveL speed, m/s or rad/s
    float m_ur_move_accel = 1.0f; // UR MoveJ/MoveL acceleration, m/s^2 or rad/s^2
};
