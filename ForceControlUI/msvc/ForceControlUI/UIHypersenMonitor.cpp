#include "UIHypersenMonitor.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"
#include <fmt/format.h>
#include <algorithm>
#include "UIUtils.h"

#define EMB_MAX_COM_NUM 40

const char *HYPERSEN_DATA_LABEL[6] = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};

UIHypersenMonitor::UIHypersenMonitor(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
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

    m_vec_hist_data.resize(HYPERSEN_SENSOR_DOF);
}

UIHypersenMonitor::~UIHypersenMonitor()
{
}

void UIHypersenMonitor::Draw()
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
    char buf[64] = {0};
    if (ImGui::CollapsingHeader(u8"串口参数设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(m_hypersen_proxy.IsSensorOK());
        stbsp_sprintf(buf, u8"%s 选择串口号", ICON_FA_ADDRESS_BOOK);
        ImGui::Text(buf);
        const char *combo_preview_value = m_com_index >= 0 ? m_vec_coms[m_com_index].c_str() : ""; // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo(u8"串口号", combo_preview_value, 0))
        {
            for (size_t n = 0; n < m_vec_coms.size(); n++)
            {
                const bool is_selected = (m_com_index == n);
                if (ImGui::Selectable(m_vec_coms[n].c_str(), is_selected))
                {
                    m_com_index = n;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Combo(u8"波特率", (int *)&m_baud_rate, u8" 9600\0 14400\0 19200\0 38400\0 56000\0 57600\0 115200\0"
                                                          u8" 230400\0 250000\0 500000\0 1000000\0 2000000\0 3000000\0 4000000\0"))
        {
        }
        ImGui::EndDisabled();
    }

    if (m_hypersen_proxy.IsSensorOK())
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.00f, 0.40f, 0.40f, 1.00f));
        if (ImGui::Button(u8"断开", ImVec2(ImGui::GetContentRegionAvail().x - 5.0f, 80.0f)))
        {
            m_hypersen_proxy.Disconnect();
        }
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::Button(u8"连接", ImVec2(ImGui::GetContentRegionAvail().x - 5.0f, 80.0f)))
        {
            if (!m_hypersen_proxy.Connect(m_vec_coms[m_com_index], m_baud_rate))
            {
                m_warning_msg = u8"可能存在以下原因：\n1. USB驱动未安装\n2. USB未连接到电脑\n3. 设备故障\n4. COM口已被其他软件打开\n5. 其他原因，请咨询技术人员";
                m_connect_failed_warning = true;
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

    ST_HypersenSensorData cur_data = m_hypersen_proxy.GetCurrentData();
    ST_HypersenSensorStatus sensor_status = m_hypersen_proxy.GetSensorStatus();
    ST_HypersenSensorInfo sensor_info = m_hypersen_proxy.GetSensorInfo();

    if (ImGui::CollapsingHeader(u8"传感控制", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(!m_hypersen_proxy.IsSensorOK());
        {
            if (sensor_status.status == 0)
            {
                if (ImGui::Button(u8"开始测量"))
                {
                    m_hypersen_proxy.StartMeasure();
                }
            }
            else if (sensor_status.status == 1)
            {
                if (ImGui::Button(u8"停止测量"))
                {
                    m_hypersen_proxy.StopMeasure();
                }
            }
            else
            {
                ImGui::Button(u8"传感器错误！");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"传感器置零"))
        {
            if (sensor_status.status == 0)
            {
                m_hypersen_proxy.ResetZero();
            }
            else
            {
                UI_WARN(u8"请先停止传感器再置零！");
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader(u8"传感监控", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Text(u8"传感器信息");
        ImGui::SameLine();
        if (ImGui::Button(u8"刷新"))
        {
            if (sensor_status.status == 0)
            {
                m_hypersen_proxy.ReadSensorDevID();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                m_hypersen_proxy.ReadVersion();
            }
        }
        {
            stbsp_sprintf(buf, "%d", sensor_info.dev_id);
            ImGui::InputText(u8"设备ID", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
            stbsp_sprintf(buf, u8"%d年%d月%d日", sensor_info.version[0], sensor_info.version[1], sensor_info.version[2]);
            ImGui::InputText(u8"固件生成日期", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
            stbsp_sprintf(buf, u8"V %d.%d.%d", sensor_info.version[3], sensor_info.version[4], sensor_info.version[5]);
            ImGui::InputText(u8"固件版本信息", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
        }
        ImGui::Text(u8"实时传感数据");
        {
            stbsp_sprintf(buf, "%d", cur_data.code);
            ImGui::InputText(u8"Exception Code", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat3(u8"Fx-Fy-Fz(N)", cur_data.data, "%.6f", ImGuiInputTextFlags_ReadOnly);
            ImGui::InputFloat3(u8"Mx-My-Mz(Nm)", cur_data.data + 3, "%.6f", ImGuiInputTextFlags_ReadOnly);
        }
        // plot hist
        {
            static float history = 30.0f;
            ImGui::SliderFloat(u8"Plot时长", &history, 1, 120, "%.1f s");

            // cur time
            float t = (float)ImGui::GetTime();

            if (ImPlot::BeginPlot(u8"##hypersenF", ImVec2(-1, 300)))
            {
                ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2);
                // ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
                {
                    std::lock_guard<std::mutex> lock(m_vec_hist_data_lock);
                    for (unsigned int i = 0; i < 3; i++)
                    {
                        if (!m_vec_hist_data[i].Data.empty())
                        {
                            ImPlot::PlotLine(HYPERSEN_DATA_LABEL[i], &m_vec_hist_data[i].Data[0].x, &m_vec_hist_data[i].Data[0].y,
                                             m_vec_hist_data[i].Data.size(), ImPlotLineFlags_None, m_vec_hist_data[i].Offset, 2 * sizeof(float));
                        }
                    }
                }
                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }

            if (ImPlot::BeginPlot(u8"##hypersenT", ImVec2(-1, 300)))
            {
                ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2);
                // ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
                {
                    std::lock_guard<std::mutex> lock(m_vec_hist_data_lock);
                    for (unsigned int i = 3; i < HYPERSEN_SENSOR_DOF; i++)
                    {
                        if (!m_vec_hist_data[i].Data.empty())
                        {
                            ImPlot::PlotLine(HYPERSEN_DATA_LABEL[i], &m_vec_hist_data[i].Data[0].x, &m_vec_hist_data[i].Data[0].y,
                                             m_vec_hist_data[i].Data.size(), ImPlotLineFlags_None, m_vec_hist_data[i].Offset, 2 * sizeof(float));
                        }
                    }
                }
                ImPlot::PopStyleVar(1);
                ImPlot::EndPlot();
            }
        }
    }

    ImGui::End();
}