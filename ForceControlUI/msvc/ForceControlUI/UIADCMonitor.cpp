#include "UIADCMonitor.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "UIConnectionSettings.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"
#include <algorithm>

UIADCMonitor::UIADCMonitor(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
{
    // 从UIConnectionSettings对象获取ModbusProxy指针并赋值给m_modbus
    UIMainWindow *uimain_win = dynamic_cast<UIMainWindow *>(main_win);
    UIWindowManagerPtr win_mng = std::dynamic_pointer_cast<UIWindowManager>(uimain_win->GetWindowManager());
    m_modbus = win_mng->GetConnectionSettings()->GetModbusProxy();
    // 注册数据回调函数
    m_modbus->RegisterDataCallback(std::bind(&UIADCMonitor::OnRegDataCB, this, std::placeholders::_1, std::placeholders::_2));
    // 初始化历史数据对象
    m_vec_hist_data.resize(ADC_CHANNEL_NUM);
    // 创建序列化线程
    m_serialize_thread = std::thread(&UIADCMonitor::SerializeThreadFunc, this);
}

UIADCMonitor::~UIADCMonitor()
{
    m_serialize_run_flag = false;
    if (m_serialize_thread.joinable())
    {
        m_serialize_thread.join();
    }
}

void UIADCMonitor::Draw()
{
    if (!m_show)
    {
        return;
    }
    char buf[256] = {0};
    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    // 拷贝input数据
    adc_info_t adc_info = {0};
    {
        std::lock_guard<std::mutex> lock(m_adc_info_lock);
        adc_info = m_adc_info;
    }
    
    if (ImGui::CollapsingHeader(u8"采样监控", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int slave_id = m_monitor_slave_id;
        stbsp_sprintf(buf, u8"监控从站地址[1,10]");
        if (ImGui::InputInt(buf, &slave_id, 1, 2))
        {
            slave_id = std::clamp(slave_id, 1, 10);
            m_monitor_slave_id = slave_id;
        }

        ImGui::InputScalar(u8"采样时间(ms)", ImGuiDataType_S16, &adc_info.adc_dt, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        for (int i = 0; i < ADC_CHANNEL_NUM; i++)
        {
            stbsp_sprintf(buf, u8"通道%d采样电压(mV)", i + 1);
            float mv = adc_info.channel_data[i] / 10.0f;
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
            std::vector<ScrollingBuffer> vec_hist_data(ADC_CHANNEL_NUM);
            {
                std::lock_guard<std::mutex> lock(m_vec_hist_data_lock);
                vec_hist_data = m_vec_hist_data;
            }

            if (ImPlot::BeginPlot("##adc_plot", ImVec2(-1, 350)))
            {
                ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2);
                // ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
                {
                    for (unsigned int i = 0; i < ADC_CHANNEL_NUM; i++)
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
        ImGui::BeginDisabled(!m_modbus->IsConnected());
        if (!m_start_serialize)
        {
            stbsp_sprintf(buf, u8"%s 开始采集", ICON_FA_CIRCLE_PLAY);
            if (ImGui::Button(buf))
            {
                // 清空数据
                {
                    std::lock_guard<std::mutex> lock(m_vec_adc_data_lock);
                    m_vec_adc_data.clear();
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

void UIADCMonitor::OnRegDataCB(int slave_id, const adc_info_t* adc_info)
{
    if(slave_id != m_monitor_slave_id || adc_info == nullptr)
    {
        return;
	}
    // 更新当前输入寄存器数据
    {
        std::lock_guard<std::mutex> lock(m_adc_info_lock);
		m_adc_info = *adc_info;
    }
    if (m_start_serialize)
    {
        // 采样数据序列化
        {
            std::lock_guard<std::mutex> lock(m_vec_adc_data_lock);
            m_vec_adc_data.push_back(*adc_info);
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
                m_vec_hist_data[i].AddPoint(t, adc_info->channel_data[i] / 10.0f);
            }
        }
    }
}

void UIADCMonitor::SerializeThreadFunc()
{
    FILE *fp = nullptr;
    std::vector<adc_info_t> vec_adc_data;

    while (m_serialize_run_flag)
    {
        if (m_start_serialize)
        {
            // 打开文件
            if (fp == nullptr)
            {
                // 获取当前时间，格式为%Y%m%d%H%M%S
                time_t now = time(0);
                tm *ltm = localtime(&now);
                char filename[64];
                stbsp_sprintf(filename, "adc_data_%d_%04d%02d%02d_%02d%02d%02d.txt",
                              m_monitor_slave_id.load(), ltm->tm_year + 1900, ltm->tm_mon + 1, ltm->tm_mday,
                              ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
                // 打开文件
                fp = fopen(filename, "w");
                if (fp)
                {
                    LOG_INFO(u8"数据采集开始，文件名：%s", filename);
                }
            }
            // 等待一段时间
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            // 将数据swap到临时变量
            {
                std::lock_guard<std::mutex> lock(m_vec_adc_data_lock);
                vec_adc_data.swap(m_vec_adc_data);
            }
            // 写入数据
            if (fp != nullptr)
            {
                // 写入数据
                for (const auto &data : vec_adc_data)
                {
                    for (int i = 0; i < ADC_CHANNEL_NUM; i++)
                    {
                        fprintf(fp, " %f", data.channel_data[i] / 10.0f);
                        // 添加逗号分隔符
                        if (i < ADC_CHANNEL_NUM - 1)
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
