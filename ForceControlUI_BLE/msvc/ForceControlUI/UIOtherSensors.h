#pragma once
#include "UIBaseWindow.h"
#include "SerialOtherReader.h"
#include "ModbusDef.h"
#include <imgui/imgui.h>
#include <thread>
#include <mutex>
#include <atomic>

class UIOtherSerialCon : public UIBaseWindow
{
public:
    UIOtherSerialCon(UIMainWindowBase* main_win, const char* title);
    ~UIOtherSerialCon();

    virtual void Draw() override;

    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_TOOL; }
    virtual const char* GetShowShortCut() { return "Ctrl+Shift+2"; }

protected:
    void UpdateSerialLogs();
    void AddLogLine(const std::string& line);
    void FlushPendingToDisplay(); // 将 pending 合并到 display，并做行数裁剪
protected:
    SerialOtherReader m_com1; // Serial log reader for reading serial data
    SerialOtherReader m_com2; // Serial log reader for reading serial data

    std::vector<std::string> m_vec_coms;

    bool m_connect_failed_warning = false;
    std::string m_warning_msg = "";

    // 串口输出显示相关
    std::mutex m_serial_log_mutex;        // 保护串口日志的互斥锁
    ImGuiTextBuffer m_display_buf;        // 仅 UI 线程读写
    ImVector<int> m_display_line_offsets; // 行起始偏移，首元素恒为 0
    ImGuiTextBuffer m_pending_buf;        // 生产者写入（受 m_serial_log_mutex 保护）

    int m_max_log_lines = 1000;                // 最大日志行数
    bool m_auto_scroll = true;                 // 自动滚动到底部
    std::atomic_bool m_show_timestamp = false; // 显示时间戳

    std::atomic_bool m_update_logs_exit = false; // 用于控制日志更新线程的退出
    std::thread m_update_logs_thread;            // 日志更新线程
};
