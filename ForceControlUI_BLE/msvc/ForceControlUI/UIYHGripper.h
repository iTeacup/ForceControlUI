#pragma once
#include "UIBaseWindow.h"
#include <thread>
#include <mutex>
#include <vector>
#include "ModbusProxy.h"
#include "common_datadef.h"
#include "UIPlotDef.h"
#include "UIHypersenMonitor.h"

struct ST_ExperimentData
{
    int gripper_width_mm = 10;           // 夹爪宽度mm
    int lamotor_pos[LA_MOTOR_NUM] = {0}; // 电机位置
    float adc_data[ADC_CHANNEL_NUM];     // 应变片数据
    float force_data[6];                 // 力传感器数据
};

class UIYHGripper : public UIBaseWindow
{
public:
    UIYHGripper(UIMainWindowBase *main_win, const char *title);
    ~UIYHGripper();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char *GetShowShortCut() { return "Ctrl+4"; }

protected:
    void OnRegDataCB(int slave_id, const adc_info_t *data);

    void ExperimentThreadFunc();

protected:
    ModbusProxy *m_modbus = nullptr;
    HypersenProxy *m_hypersen_proxy = nullptr;

    std::mutex m_cur_adc_lock;
    int m_slave_id = 0; // 当前从站ID
    adc_info_t m_cur_adc = {0};

    int m_gripper_width_mm = 10; // 夹爪宽度mm
    int m_wait_time_sec = 2; // 等待时间秒
    int m_motor1_begin = 2000;
    int m_motor1_end = 0;
    int m_motor2_begin = 0;
    int m_motor2_end = 2000;

    std::atomic_bool m_experiment_run_flag = false;
    std::thread m_experiment_thread;

    std::mutex m_experiment_data_lock;
    std::vector<ST_ExperimentData> m_experiment_data; // 实验数据
};
