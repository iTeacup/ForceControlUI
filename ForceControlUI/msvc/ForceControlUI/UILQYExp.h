#pragma once
#include "UIBaseWindow.h"
#include <thread>
#include <mutex>
#include <vector>
#include "ModbusProxy.h"
#include "common_datadef.h"
#include "UIPlotDef.h"
#include <CPSSRISensorDef.h>
#include <CPSOptServerDef.h>
#include <CPSAPI/CPSAPI.h>
#include "URManager.h"

class UIOptiTrack;
struct ST_LQYExpData
{
    int lamotor_pos = 0;         // 电机位置
    float adc_data[2];           // 应变片数据
    ST_SRISensorData force_data; // 力传感器数据
    ST_OptMarker markers[6];     // 光学定位数据
};

class UILQYExp : public UIBaseWindow
{
public:
    UILQYExp(UIMainWindowBase *main_win, const char *title);
    ~UILQYExp();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char *GetShowShortCut() { return "Ctrl+8"; }

    virtual void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char *data, uint32_t msg_len) override;

protected:
    void OnRegDataCB(int slave_id, const adc_info_t *data);
    void ExperimentThreadFunc();

    void ReqSensorResetZero();
protected:
    ModbusProxy *m_modbus = nullptr;
    CCPSAPI *m_cps_api = nullptr;
    URManager *m_ur_manager = nullptr;
    UIOptiTrack *m_opti_track = nullptr;

    std::mutex m_cur_adc_lock;
    int m_slave_id = 0; // 当前从站ID
    adc_info_t m_cur_adc = {0};

    std::mutex m_cur_force_lock;
    ST_SRISensorData m_cur_force = {0};

    int m_lamotor_step = 10;
    int m_wait_time_sec = 10; // 等待时间秒

    std::atomic_bool m_experiment_run_flag = false;
    std::thread m_experiment_thread;

    std::mutex m_experiment_data_lock;
    std::vector<ST_LQYExpData> m_experiment_data; // 实验数据
};
