#include "UICOMMonitor.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "UIConnectionSettings.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"
#include <algorithm>


#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "UICfgParser.h"
#include "UIUtils.h"
#include "portable-file-dialogs.h"  // 单头文件

namespace
{
bool IsValidComIndex(int index, size_t count)
{
    return index >= 0 && static_cast<size_t>(index) < count;
}
}

UICOMMonitor::UICOMMonitor(UIMainWindowBase* main_win, const char* title) : UIBaseWindow(main_win, title)
{
    // 从UIConnectionSettings对象获取ModbusProxy指针并赋值给m_modbus
    UIMainWindow* uimain_win = dynamic_cast<UIMainWindow*>(main_win);
    UIWindowManagerPtr win_mng = std::dynamic_pointer_cast<UIWindowManager>(uimain_win->GetWindowManager());
    //m_modbus = win_mng->GetConnectionSettings()->GetModbusProxy();
    //// 注册数据回调函数
    //m_modbus->RegisterDataCallback(std::bind(&UICOMMonitor::OnRegDataCB, this, std::placeholders::_1, std::placeholders::_2));
    
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
    
    // 初始化历史数据对象
    m_vec_hist_data.resize(CALIBRATION_STRAIN_CHANNEL_NUM);
    // 创建序列化线程
    m_serialize_thread = std::thread(&UICOMMonitor::SerializeThreadFunc, this);
}

UICOMMonitor::~UICOMMonitor()
{
    m_serialize_run_flag = false;
    if (m_serialize_thread.joinable())
    {
        m_serialize_thread.join();
    }
    m_com1.CloseSerialPort(); // 确保串口连接被关闭
    m_com2.CloseSerialPort(); // 确保串口连接被关闭
}

// ========== 静态工具方法实现 ==========

std::string UICOMMonitor::RemoveNewlines(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (char c : s) {
        if (c != '\n' && c != '\r') {
            result += c;
        }
    }
    return result;
}

void UICOMMonitor::ParseFloatArray(const std::string& input) {
    std::vector<float> result;
    std::stringstream ss(input);
    std::string token;

    while (std::getline(ss, token, ',')) {
        // 去掉可能的换行符和空格
        token.erase(
            std::remove_if(token.begin(), token.end(),
                [](char c) {
                    return c == '\n' || c == '\r' || c == ' ';
                }),
            token.end()
        );

        if (!token.empty()) {
            try {
                result.push_back(std::stof(token));
            }
            catch (...) {
                // 解析失败，跳过
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_sensors_data_lock);
        const int channel_count = std::min(static_cast<int>(result.size()), CALIBRATION_STRAIN_CHANNEL_NUM);
        for (int i = 0; i < channel_count; i++)
        {
            m_sensors.com_data[i] = result[i];
        }
    }

    if (m_start_serialize)
    {
        // 采样数据序列化
        {
            std::lock_guard<std::mutex> lock(m_com_data_lock);
            m_com_data.push_back(m_sensors);
        }
    }
    { // 绘图专用
        // 获取当前程序时间，转化为浮点数，单位为秒
        auto now = std::chrono::steady_clock::now();
        float t = std::chrono::duration<float>(now - m_start_time).count();

        // 更新数据队列
        {
            std::lock_guard<std::mutex> lock(m_vec_hist_data_lock);
            for (int i = 0; i < ADC_CHANNEL_NUM; i++)
            {
                m_vec_hist_data[i].AddPoint(t, m_sensors.com_data[i] / 10.0f);
            }
        }
    }

}

void UICOMMonitor::Draw()
{
    if (!m_show)
    {
        return;
    }
    char buf[256] = { 0 };
    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    // 拷贝input数据
    ST_ComData ComInfo = { 0 };
    {
        std::lock_guard<std::mutex> lock(m_sensors_data_lock);
        ComInfo = m_sensors;
    }

    ST_SerialCfg* serial1_cfg = &(g_cfg->m_com1_serial_cfg);
    if (ImGui::CollapsingHeader(u8"串口1参数设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(m_com1.IsConnected());
        stbsp_sprintf(buf, u8"%s 选择串口号1", ICON_FA_ADDRESS_BOOK);
        ImGui::Text(buf);
        const char* combo_preview_value = IsValidComIndex(serial1_cfg->com_index, m_vec_coms.size()) ? m_vec_coms[serial1_cfg->com_index].c_str() : u8"不使用"; // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo(u8"串口号1", combo_preview_value, 0))
        {
            const bool is_disabled = serial1_cfg->com_index < 0;
            if (ImGui::Selectable(u8"不使用", is_disabled))
            {
                serial1_cfg->com_index = -1;
            }
            if (is_disabled)
                ImGui::SetItemDefaultFocus();

            for (size_t n = 0; n < m_vec_coms.size(); n++)
            {
                const bool is_selected = (serial1_cfg->com_index == static_cast<int>(n));
                if (ImGui::Selectable(m_vec_coms[n].c_str(), is_selected))
                {
                    serial1_cfg->com_index = static_cast<int>(n);
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
        const char* combo_preview_value2 = IsValidComIndex(serial2_cfg->com_index, m_vec_coms.size()) ? m_vec_coms[serial2_cfg->com_index].c_str() : u8"不使用"; // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo(u8"串口号2", combo_preview_value2, 0))
        {
            const bool is_disabled = serial2_cfg->com_index < 0;
            if (ImGui::Selectable(u8"不使用", is_disabled))
            {
                serial2_cfg->com_index = -1;
            }
            if (is_disabled)
                ImGui::SetItemDefaultFocus();

            for (size_t n = 0; n < m_vec_coms.size(); n++)
            {
                const bool is_selected = (serial2_cfg->com_index == static_cast<int>(n));
                if (ImGui::Selectable(m_vec_coms[n].c_str(), is_selected))
                {
                    serial2_cfg->com_index = static_cast<int>(n);
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
   

    if (m_com1.IsConnected() || m_com2.IsConnected())
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
            const bool use_com1 = IsValidComIndex(serial1_cfg->com_index, m_vec_coms.size());
            const bool use_com2 = IsValidComIndex(serial2_cfg->com_index, m_vec_coms.size());

            if (!use_com1 && !use_com2)
            {
                m_warning_msg = u8"请至少选择一路有效的串口号";
                m_connect_failed_warning = true;
            }
            else if (use_com1 && use_com2 && serial1_cfg->com_index == serial2_cfg->com_index)
            {
                m_warning_msg = u8"串口1和串口2不能选择同一个COM口";
                m_connect_failed_warning = true;
            }
            else
            {
                bool open_ok = true;
                if (use_com1)
                {
                    open_ok = m_com1.OpenSerialPort(m_vec_coms[serial1_cfg->com_index], serial1_cfg->baud_rate);
                }
                if (open_ok && use_com2)
                {
                    open_ok = m_com2.OpenSerialPort(m_vec_coms[serial2_cfg->com_index], serial2_cfg->baud_rate);
                }

                if (!open_ok)
                {
                    m_com1.CloseSerialPort();
                    m_com2.CloseSerialPort();
                    m_warning_msg = u8"打开串口失败，请检查串口设置";
                    m_connect_failed_warning = true;
                }
                else
                {
                    {
                        std::lock_guard<std::mutex> lock(m_coms_data_lock);
                        m_line1.clear();
                        m_line2.clear();
                    }
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
                                    //std::string line1consume = m_com1.GetLog();
                                    //std::string line2consume = m_com2.GetLog();
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

    if (ImGui::CollapsingHeader(u8"COM监控", ImGuiTreeNodeFlags_DefaultOpen))
    {
        //int slave_id = m_monitor_slave_id;
        //stbsp_sprintf(buf, u8"监控从站地址[1,10]");
        //if (ImGui::InputInt(buf, &slave_id, 1, 2))
        //{
        //    slave_id = std::clamp(slave_id, 1, 10);
        //    m_monitor_slave_id = slave_id;
        //}

        //ImGui::InputScalar(u8"采样时间(ms)", ImGuiDataType_S16, &adc_info.adc_dt, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; i++)
        {
            stbsp_sprintf(buf, u8"通道%d采样电压(mV)", i + 1);
            float mv = ComInfo.com_data[i] ;
            ImGui::InputFloat(buf, &mv, 0.0f, 0.0f, "%.1f", ImGuiInputTextFlags_ReadOnly);
        }

        // plot hist
        {
            static float history = 30.0f;
            ImGui::SliderFloat(u8"Plot时长", &history, 1, 120, "%.1f s");

            // static ImPlotAxisFlags flags = ImPlotAxisFlags_None /*ImPlotAxisFlags_NoTickLabels*/;
            //  cur time
            // 获取当前程序时间，转化为浮点数，单位为秒
            auto now = std::chrono::steady_clock::now();
            float t = std::chrono::duration<float>(now - m_start_time).count();

            // copy hist data
            std::vector<ScrollingBuffer> vec_hist_data(CALIBRATION_STRAIN_CHANNEL_NUM);
            {
                std::lock_guard<std::mutex> lock(m_vec_hist_data_lock);
                vec_hist_data = m_vec_hist_data;
            }

            if (ImPlot::BeginPlot("##com_plot", ImVec2(-1, 350)))
            {
                ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2);
                // ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
                {
                    for (unsigned int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; i++)
                    {
                        if (vec_hist_data[i].Data.size())
                        {
                            stbsp_sprintf(buf, "L%d", i + 1);
                            ImPlot::PlotLine(buf, &vec_hist_data[i].Data[0].x, &vec_hist_data[i].Data[0].y,
                                vec_hist_data[i].Data.size(), ImPlotLineFlags_None, vec_hist_data[i].Offset, 2 * sizeof(float));
                        }
                    }
                }
                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        }
    }
    if (ImGui::CollapsingHeader(u8"数据存储", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(!m_com1.IsConnected() && !m_com2.IsConnected());
        if (!m_start_serialize)
        {
            stbsp_sprintf(buf, u8"%s 开始采集", ICON_FA_CIRCLE_PLAY);
            if (ImGui::Button(buf))
            {
                // 清空数据
                {
                    std::lock_guard<std::mutex> lock(m_com_data_lock);
                    m_com_data.clear();
                }
                // 开始采样
                m_start_serialize = true;
            }
        }
        else
        {
            stbsp_sprintf(buf, u8"%s 停止采集", ICON_FA_CIRCLE_STOP);
            if (ImGui::Button(buf))
            {
                m_start_serialize = false;
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
}

//void UICOMMonitor::OnRegDataCB(int slave_id, const adc_info_t* adc_info)
//{
//    if (slave_id != m_monitor_slave_id || adc_info == nullptr)
//    {
//        return;
//    }
//    // 更新当前输入寄存器数据
//    {
//        std::lock_guard<std::mutex> lock(m_adc_info_lock);
//        m_adc_info = *adc_info;
//    }
//    if (m_start_serialize)
//    {
//        // 采样数据序列化
//        {
//            std::lock_guard<std::mutex> lock(m_vec_adc_data_lock);
//            m_vec_adc_data.push_back(*adc_info);
//        }
//    }
//    { // 绘图专用
//        // 获取当前程序时间，转化为浮点数，单位为秒
//        auto now = std::chrono::steady_clock::now();
//        float t = std::chrono::duration<float>(now - m_start_time).count();
//
//        // 更新数据队列
//        {
//            std::lock_guard<std::mutex> lock(m_vec_hist_data_lock);
//            for (int i = 0; i < ADC_CHANNEL_NUM; i++)
//            {
//                m_vec_hist_data[i].AddPoint(t, adc_info->channel_data[i] / 10.0f);
//            }
//        }
//    }
//}

void UICOMMonitor::SerializeThreadFunc()
{
    FILE* fp = nullptr;
    std::vector<ST_ComData> vec_adc_data;

    while (m_serialize_run_flag)
    {

        std::string Line_combined;

        // UpdateSerialLogs();
        {
            std::lock_guard<std::mutex> lock(m_coms_data_lock);
            // 直接传入多个 string 变量，不需要 vector
            Line_combined = JoinAndAddFinalNewline(m_line1, m_line2, "\r\n");

        }

        ParseFloatArray(Line_combined);

        if (m_start_serialize)
        {
            // 打开文件
            if (fp == nullptr)
            {
                // 获取当前时间，格式为%Y%m%d%H%M%S
                time_t now = time(0);
                tm* ltm = localtime(&now);
                char filename[64];
                stbsp_sprintf(filename, "com_data_%04d%02d%02d_%02d%02d%02d.txt",
                     ltm->tm_year + 1900, ltm->tm_mon + 1, ltm->tm_mday,
                    ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
                // 打开文件
                fp = fopen(filename, "w");
                if (fp)
                {
                    LOG_INFO(u8"数据采集开始，文件名：%s", filename);
                }
            }
            // 等待一段时间
            //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            // 将数据swap到临时变量
            {
                std::lock_guard<std::mutex> lock(m_com_data_lock);
                vec_adc_data.swap(m_com_data);
            }
            // 写入数据
            if (fp != nullptr)
            {
                // 写入数据
                for (const auto& data : vec_adc_data)
                {
                    for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; i++)
                    {
                        fprintf(fp, " %f", data.com_data[i] / 10.0f);
                        // 添加逗号分隔符
                        if (i < CALIBRATION_STRAIN_CHANNEL_NUM - 1)
                        {
                            fprintf(fp, ",");
                        }
                    }
                    fprintf(fp, "\n");
                }
                fflush(fp);
                if (!vec_adc_data.empty())
                {
                    LOG_INFO(u8"写入%d条数据...", vec_adc_data.size());
                }
                // 清空缓存
                vec_adc_data.clear();
            }
        }
        else
        {
            // 关闭文件
            if (fp != nullptr)
            {
                fclose(fp);
                fp = nullptr;
                LOG_INFO(u8"数据采集完成");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    // 关闭文件
    if (fp != nullptr)
    {
        fclose(fp);
        fp = nullptr;
    }
}

void UICOMMonitor::UpdateSerialLogs()
{
    // 步骤1：先在外部无锁获取数据（让串口类自己管理内部锁）
    std::string new_line1;
    std::string new_line2;
    if (m_com1.IsConnected())
    {
        new_line1 = m_com1.GetLog();
    }
    if (m_com2.IsConnected())
    {
        new_line2 = m_com2.GetLog();
    }
    //LOG_INFO("%s", new_line2.c_str());

    // 步骤2：只拿保护成员变量的锁，快速赋值
    std::lock_guard<std::mutex> lock(m_coms_data_lock);

    // 避免空字符串覆盖有效数据（根据业务需要选择）
    if (!new_line1.empty()) {
        m_line1 = std::move(new_line1);  // 移动语义，避免拷贝
    }
    if (!new_line2.empty()) {
        m_line2 = std::move(new_line2);
    }


}
