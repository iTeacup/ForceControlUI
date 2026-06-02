#include "UIOtherSensors.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "IconsFontAwesome6.h"
#include "stb/stb_sprintf.h"
#include "UIUtils.h"
#include <fmt/format.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "UICfgParser.h"

#define EMB_MAX_COM_NUM 40

UIOtherSerialCon::UIOtherSerialCon(UIMainWindowBase* main_win, const char* title) : UIBaseWindow(main_win, title)
{
    for (int i = 0; i < EMB_MAX_COM_NUM; i++)
    {
        if (i >= 9)
        {
            m_vec_coms.push_back(fmt::format("\\\\.\\COM{}", i + 1));
        }
        else
        {
            m_vec_coms.push_back(fmt::format("COM{}", i + 1));
        }
    }
    // 使用 ImGuiTextBuffer 行偏移：首元素为 0
    m_display_line_offsets.clear();
    m_display_line_offsets.push_back(0);
}

UIOtherSerialCon::~UIOtherSerialCon()
{
    m_update_logs_exit = true; // 设置退出标志，停止日志更新线程
    if (m_update_logs_thread.joinable())
    {
        m_update_logs_thread.join(); // 等待线程结束
    }
    m_com1.CloseSerialPort(); // 确保串口连接被关闭
    m_com2.CloseSerialPort(); // 确保串口连接被关闭
}

// 基础函数：去掉字符串中的所有换行符（\n 和 \r）
std::string removeNewlines(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (char c : s) {
        if (c != '\n' && c != '\r') {
            result += c;
        }
    }
    return result;
}

// 可变参数模板：处理任意数量的字符串，去掉各自内部换行符，整体末尾加一个换行符
template<typename... Args>
std::string joinAndAddFinalNewline(Args&&... args) {
    std::string result;

    // 预分配空间（估算）
    size_t total_size = (0 + ... + std::string(std::forward<Args>(args)).size());
    result.reserve(total_size + 1);  // +1 为最终换行符

    // 折叠表达式：处理每个参数，去掉换行符后追加
    (result += ... += removeNewlines(std::forward<Args>(args)));

    // 只在最后加一个换行符
    result += '\n';
    return result;
}

void UIOtherSerialCon::UpdateSerialLogs()
{
    std::string line1 = m_com1.GetLog();
    std::string line2 = m_com2.GetLog();
    // 直接传入多个 string 变量，不需要 vector
    std::string Line_combined = joinAndAddFinalNewline(line1, line2, "\r\n");
    
    while (!line1.empty())
    {
        AddLogLine(line1);
        line1 = m_com1.GetLog(); // 获取下一行数据
        line2 = m_com2.GetLog();
        //Line_combined = joinAndAddFinalNewline(line1, line2, "\r\n");
    }
}

void UIOtherSerialCon::AddLogLine(const std::string& line)
{
    std::string log_line;

    if (m_show_timestamp)
    {
        // 添加时间戳
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) %
            1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "[%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        log_line = oss.str() + line;
    }
    else
    {
        log_line = line;
    }

    // 规范换行：去掉尾部 \r\n，再补一个 \n，保证行偏移可按 '\n' 识别
    while (!log_line.empty() && (log_line.back() == '\r' || log_line.back() == '\n'))
        log_line.pop_back();
    log_line.push_back('\n');

    // 生产者只写 pending，减少 UI 线程锁竞争
    {
        std::lock_guard<std::mutex> lock(m_serial_log_mutex);
        m_pending_buf.append(log_line.c_str());
    }
}

// 合并 pending -> display，并按 m_max_log_lines 裁剪
void UIOtherSerialCon::FlushPendingToDisplay()
{
    // 1) 合并 pending
    int old_size = m_display_buf.Buf.Size;
    {
        std::lock_guard<std::mutex> lock(m_serial_log_mutex);
        if (m_pending_buf.Buf.Size > 0)
        {
            m_display_buf.append(m_pending_buf.c_str());
            m_pending_buf.clear();
        }
    }
    // 2) 增量更新行偏移（只扫描新追加的部分）
    for (int i = old_size; i < m_display_buf.Buf.Size; ++i)
    {
        if (m_display_buf.Buf[i] == '\n')
            m_display_line_offsets.push_back(i + 1);
    }
    if (m_display_line_offsets.empty())
        m_display_line_offsets.push_back(0);

    // 3) 行数限制裁剪（批量从头裁掉多余的行，保留最后 m_max_log_lines 行）
    const int allowed = m_max_log_lines + 1; // +1 保持首元素 0 的约定
    // 加入滞后阈值，避免每帧重建大缓冲
    const int lag_threshold = 100; // 100 行的滞后阈值
    if (m_display_line_offsets.Size > allowed + lag_threshold)
    {
        const int remove_lines = m_display_line_offsets.Size - allowed;
        const int cut_pos = m_display_line_offsets[remove_lines]; // 新的文本起始字节位置

        // 重建 display_buf（保留尾部）
        ImGuiTextBuffer new_buf;
        new_buf.append(m_display_buf.c_str() + cut_pos);
        m_display_buf = std::move(new_buf);

        // 重建行偏移（所有偏移减去 cut_pos，并去掉前 remove_lines 个偏移）
        ImVector<int> new_offsets;
        new_offsets.reserve(allowed);
        new_offsets.push_back(0);
        for (int i = remove_lines + 1; i < m_display_line_offsets.Size; ++i)
            new_offsets.push_back(m_display_line_offsets[i] - cut_pos);
        m_display_line_offsets = std::move(new_offsets);
    }
}

void UIOtherSerialCon::Draw()
{
    if (!m_show)
    {
        return;
    }
    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    char buf[64] = { 0 };
    ST_SerialCfg* serial1_cfg = &(g_cfg->m_com1_serial_cfg);
    if (ImGui::CollapsingHeader(u8"串口1参数设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(m_com1.IsConnected());
        stbsp_sprintf(buf, u8"%s 选择串口号1", ICON_FA_ADDRESS_BOOK);
        ImGui::Text(buf);
        const char* combo_preview_value = serial1_cfg->com_index >= 0 ? m_vec_coms[serial1_cfg->com_index].c_str() : ""; // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo(u8"串口号1", combo_preview_value, 0))
        {
            for (size_t n = 0; n < m_vec_coms.size(); n++)
            {
                const bool is_selected = (serial1_cfg->com_index == n);
                if (ImGui::Selectable(m_vec_coms[n].c_str(), is_selected))
                {
                    serial1_cfg->com_index = n;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Combo(u8"串口1波特率", (int*)&serial1_cfg->baud_rate, u8" 9600\0 14400\0 19200\0 38400\0 56000\0 57600\0 115200\0"
            u8" 230400\0 250000\0 500000\0 1000000\0 2000000\0 3000000\0 4000000\0"))
        {
        }
        ImGui::EndDisabled();
    }

    ST_SerialCfg* serial2_cfg = &(g_cfg->m_com2_serial_cfg);
    if (ImGui::CollapsingHeader(u8"串口2参数设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(m_com2.IsConnected());
        stbsp_sprintf(buf, u8"%s 选择串口号2", ICON_FA_ADDRESS_BOOK);
        ImGui::Text(buf);
        const char* combo_preview_value2 = serial2_cfg->com_index >= 0 ? m_vec_coms[serial2_cfg->com_index].c_str() : ""; // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo(u8"串口号2", combo_preview_value2, 0))
        {
            for (size_t n = 0; n < m_vec_coms.size(); n++)
            {
                const bool is_selected = (serial2_cfg->com_index == n);
                if (ImGui::Selectable(m_vec_coms[n].c_str(), is_selected))
                {
                    serial2_cfg->com_index = n;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Combo(u8"串口2波特率", (int*)&serial2_cfg->baud_rate, u8" 9600\0 14400\0 19200\0 38400\0 56000\0 57600\0 115200\0"
            u8" 230400\0 250000\0 500000\0 1000000\0 2000000\0 3000000\0 4000000\0"))
        {
        }
        ImGui::EndDisabled();
    }

    if (m_com1.IsConnected()|| m_com2.IsConnected())
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.00f, 0.40f, 0.40f, 1.00f));
        if (ImGui::Button(u8"断开", ImVec2(ImGui::GetContentRegionAvail().x - 5.0f, 80.0f)))
        {
            m_update_logs_exit = true; // 设置退出标志，停止日志更新线程
            if (m_update_logs_thread.joinable())
            {
                m_update_logs_thread.join(); // 等待线程结束
            }
            m_com1.CloseSerialPort();
            m_com2.CloseSerialPort();
        }
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::Button(u8"连接", ImVec2(ImGui::GetContentRegionAvail().x - 5.0f, 80.0f)))
        {
            if (serial1_cfg->com_index < 0 || serial1_cfg->com_index >= m_vec_coms.size())
            {
                m_warning_msg = u8"请选择有效的串口号";
                m_connect_failed_warning = true;
            }
            else
            {
                if (!m_com1.OpenSerialPort(m_vec_coms[serial1_cfg->com_index], serial1_cfg->baud_rate)|| !m_com2.OpenSerialPort(m_vec_coms[serial2_cfg->com_index], serial2_cfg->baud_rate))
                {
                    m_warning_msg = u8"打开串口失败，请检查串口设置";
                    m_connect_failed_warning = true;
                }
                else
                {
                    // 启动日志更新线程
                    if (!m_update_logs_thread.joinable())
                    {
                        m_update_logs_exit = false;
                        m_update_logs_thread = std::thread([this]()
                            {
                                while (!m_update_logs_exit)
                                {
                                    UpdateSerialLogs();
                                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                } });
                    }
                }
                // save cfg
                g_cfg->SaveCfg();
            }
        }
        if (m_connect_failed_warning)
        {
            stbsp_sprintf(buf, u8"%s 连接失败", ICON_FA_TRIANGLE_EXCLAMATION);
            int ret = UIUtils::Inst()->ShowMessageBox(buf, m_warning_msg.c_str(), E_BTN_OK);
            if (ret != -1)
            {
                m_connect_failed_warning = false;
            }
        }
    }

    // 串口输出显示区域
    if (ImGui::CollapsingHeader(u8"串口输出", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 先把后台 pending 合并到 display，保证本帧显示最新
        FlushPendingToDisplay();

        // 控制选项
        ImGui::Checkbox(u8"自动滚动", &m_auto_scroll);
        ImGui::SameLine();
        // Checkbox 操作原子变量
        bool ts = m_show_timestamp.load(std::memory_order_relaxed);
        if (ImGui::Checkbox(u8"显示时间", &ts))
            m_show_timestamp.store(ts, std::memory_order_relaxed);
        ImGui::SameLine();
        if (ImGui::Button(u8"清空日志"))
        {
            // 清空 display 和 pending
            {
                std::lock_guard<std::mutex> lock(m_serial_log_mutex);
                m_pending_buf.clear();
            }
            m_display_buf.clear();
            m_display_line_offsets.clear();
            m_display_line_offsets.push_back(0);
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"复制日志"))
        {
            ImGui::SetClipboardText(m_display_buf.c_str());
        }

        ImGui::SliderInt(u8"日志行数限制", &m_max_log_lines, 1, 10000, u8"%d 行");

        // 显示日志统计信息
        const int line_count = (m_display_line_offsets.Size > 0) ? (m_display_line_offsets.Size - 1) : 0;
        ImGui::Text(u8"日志行数: %d / %d", line_count, m_max_log_lines);

        // 滚动文本区域
        ImGui::Separator();
        ImVec2 outer_size = ImVec2(0.0f, -1.0f);

        if (ImGui::BeginChild("ScrollingRegion", outer_size, true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGuiListClipper clipper;
            clipper.Begin(line_count);
            const char* buf_start = m_display_buf.c_str();
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; ++line_no)
                {
                    const char* line_start = buf_start + m_display_line_offsets[line_no];
                    const char* line_end = buf_start + m_display_line_offsets[line_no + 1] - 1; // 去掉 '\n'
                    ImGui::TextUnformatted(line_start, line_end);
                }
            }
            clipper.End();
            // 自动滚动到底部
            if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}
