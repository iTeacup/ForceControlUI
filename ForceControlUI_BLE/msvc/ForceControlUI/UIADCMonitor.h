#pragma once
#include "UIBaseWindow.h"
#include <thread>
#include <mutex>
#include <vector>
#include "ModbusProxy.h"
#include "common_datadef.h"
#include "UIPlotDef.h"


class UIADCMonitor :
    public UIBaseWindow
{
public:
    UIADCMonitor(UIMainWindowBase* main_win, const char* title);
    ~UIADCMonitor();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char* GetShowShortCut() { return "Ctrl+2"; }
protected:
    void OnRegDataCB(int slave_id, const adc_info_t* adc_info);

    void SerializeThreadFunc();
protected:
    ModbusProxy* m_modbus = nullptr;
    // 监控的从机ID
	std::atomic_int m_monitor_slave_id = 1; 

    std::mutex m_adc_info_lock;
	adc_info_t m_adc_info = { 0 };

    // 程序启动时间
    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
    std::mutex m_vec_hist_data_lock;
    std::vector<ScrollingBuffer> m_vec_hist_data;

    // 开始序列化标志
    bool m_start_serialize = false;
    std::thread m_serialize_thread;
    std::atomic_bool m_serialize_run_flag = true;

    // 采样数据
    std::mutex m_vec_adc_data_lock;
    std::vector<adc_info_t> m_vec_adc_data;
};

