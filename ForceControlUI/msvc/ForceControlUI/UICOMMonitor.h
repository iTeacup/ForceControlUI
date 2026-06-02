#pragma once
#pragma once
#include "UIBaseWindow.h"
#include <thread>
#include <mutex>
#include <vector>
//#include "ModbusProxy.h"
#include "SerialOtherReader.h"
#include "common_datadef.h"
#include "UIPlotDef.h"
#include <fmt/format.h>


#define CALIBRATION_STRAIN_CHANNEL_NUM 8 // ADC通道数
#define EMB_MAX_COM_NUM 40


struct ST_ComData
{
    float com_data[CALIBRATION_STRAIN_CHANNEL_NUM] = { 0 };
};


class UICOMMonitor :
    public UIBaseWindow
{
public:
    UICOMMonitor(UIMainWindowBase* main_win, const char* title);
    ~UICOMMonitor();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char* GetShowShortCut() { return "Ctrl+2"; }
protected:
    //void OnRegDataCB(int slave_id, const adc_info_t* adc_info);

    void SerializeThreadFunc();
protected:
    //ModbusProxy* m_modbus = nullptr;
    //// 监控的从机ID
    //std::atomic_int m_monitor_slave_id = 1;

    void UpdateSerialLogs();
    //ModbusProxy* m_modbus = nullptr;
    //HypersenProxy* m_hypersen_proxy = nullptr;

    SerialOtherReader m_com1; // Serial log reader for reading serial data
    SerialOtherReader m_com2; // Serial log reader for reading serial data

    std::vector<std::string> m_vec_coms;

    ImVector<int> m_display_line_offsets; // 行起始偏移，首元素恒为 0

    /**
     * @brief 去掉字符串中的所有换行符（\n 和 \r）
     * @param s 输入字符串
     * @return 处理后的字符串
     */
    static std::string RemoveNewlines(const std::string& s);


    /**
     * @brief 拼接多个字符串，去掉各自内部换行符，整体末尾加一个换行符
     * @tparam Args 可变参数类型
     * @param args 多个字符串参数
     * @return 拼接后的字符串
     */
    template<typename... Args>
    static std::string JoinAndAddFinalNewline(Args&&... args);

    /**
     * @brief 解析逗号分隔的浮点数字符串，如 "600,500,321.5,321\n"
     * @param input 输入字符串
     * @return float数组
     */
    void ParseFloatArray(const std::string& input);

    std::atomic_bool m_update_logs_exit = false; // 用于控制日志更新线程的退出
    std::thread m_update_logs_thread;            // 日志更新线程
    bool m_connect_failed_warning = false;
    std::string m_warning_msg = "";

    ST_ComData m_sensors;



    //std::mutex m_adc_info_lock;
    //adc_info_t m_adc_info = { 0 };

    std::mutex m_coms_data_lock;
    std::mutex m_sensors_data_lock;
    std::string m_line1;
    std::string m_line2;

    // 程序启动时间
    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
    std::mutex m_vec_hist_data_lock;
    std::vector<ScrollingBuffer> m_vec_hist_data;

    // 开始序列化标志
    bool m_start_serialize = false;
    std::thread m_serialize_thread;
    std::atomic_bool m_serialize_run_flag = true;

    //// 采样数据
    //std::mutex m_vec_adc_data_lock;
    //std::vector<adc_info_t> m_vec_adc_data;

    std::mutex m_com_data_lock;
    std::vector<ST_ComData> m_com_data; // 实验数据
};

template<typename... Args>
std::string UICOMMonitor::JoinAndAddFinalNewline(Args&&... args) {
    std::string result;

    // 预分配空间
    size_t total_size = (0 + ... + std::string(std::forward<Args>(args)).size());
    result.reserve(total_size + 1);

    // 折叠表达式：处理每个参数
    (result += ... += RemoveNewlines(std::forward<Args>(args)));

    result += '\n';
    return result;
}
