#pragma once
#include "UIBaseWindow.h"
#include "SerialADCReader.h"
#include "ModbusDef.h"
#include <imgui/imgui.h>
#include <thread>
#include <mutex>
#include <atomic>

class UIADCSerialCon : public UIBaseWindow
{
public:
    UIADCSerialCon(UIMainWindowBase *main_win, const char *title);
    ~UIADCSerialCon();

    virtual void Draw() override;

    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_TOOL; }
    virtual const char *GetShowShortCut() { return "Ctrl+Shift+3"; }

protected:
    void ADCDataCallback(const adc_info_t *adc_data);

    void FlushPendingToDisplay(); // 将 pending 合并到 display，并做行数裁
protected:
    SerialADCReader m_serial_reader; // Serial log reader for reading serial data

    std::vector<std::string> m_vec_coms;

    bool m_connect_failed_warning = false;
    std::string m_warning_msg = "";

    // 串口输出显示相关
    std::mutex m_serial_log_mutex;        // 保护串口日志的互斥锁
    ImGuiTextBuffer m_display_buf;        // 仅 UI 线程读写
    ImVector<int> m_display_line_offsets; // 行起始偏移，首元素恒为 0
    ImGuiTextBuffer m_pending_buf;        // 生产者写入（受 m_serial_log_mutex 保护）

    int m_max_lines = 1000;                    // 最大行数
    bool m_auto_scroll = true;                 // 自动滚动到底部
    std::atomic_bool m_show_timestamp = false; // 显示时间戳
    std::atomic_bool m_show_data = false;        // 显示数据
};
