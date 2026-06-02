#include "UIConnectionSettings.h"
#include "imgui/imgui.h"
#include "imgui_toggle/imgui_toggle.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "IconsFontAwesome6.h"
#include "stb/stb_sprintf.h"
#include "UIUtils.h"
#include <fmt/format.h>
#include <algorithm>
#include "Logger.h"
#include "UICfgParser.h"

#define EMB_MAX_COM_NUM 40
#define EMB_MAX_SLAVE_ID_NUM 10

UIConnectionSettings::UIConnectionSettings(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
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
    for (int i = 0; i < EMB_MAX_SLAVE_ID_NUM; i++)
    {
        m_slave_ids.push_back(i + 1);
    }
}

UIConnectionSettings::~UIConnectionSettings()
{
}

void UIConnectionSettings::Draw()
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
    ST_MBCfg* mb_cfg = &g_cfg->m_mb_cfg;

    stbsp_sprintf(buf, u8"%s 通讯类型选择", ICON_FA_NETWORK_WIRED);
    ImGui::Text(buf);
    ImGui::BeginDisabled(m_modbus.IsConnected());
    if (ImGui::Combo(u8"##类型", (int *)&m_con_type, u8" RTU\0 TCP\0"))
    {
    }
    ImGui::EndDisabled();
    if (ImGui::CollapsingHeader(u8"连接设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(m_modbus.IsConnected());
        if (m_con_type == E_RTU)
        {
            stbsp_sprintf(buf, u8"%s 选择串口号", ICON_FA_ADDRESS_BOOK);
            ImGui::Text(buf);
            const char *combo_preview_value = mb_cfg->com_index >= 0 ? m_vec_coms[mb_cfg->com_index].c_str() : ""; // Pass in the preview value visible before opening the combo (it could be anything)
            if (ImGui::BeginCombo(u8"串口号", combo_preview_value, 0))
            {
                for (size_t n = 0; n < m_vec_coms.size(); n++)
                {
                    const bool is_selected = (mb_cfg->com_index == n);
                    if (ImGui::Selectable(m_vec_coms[n].c_str(), is_selected))
                    {
                        mb_cfg->com_index = n;
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::Combo(u8"波特率", (int *)&mb_cfg->baud_rate, u8" 9600\0 14400\0 19200\0 38400\0 56000\0 57600\0 115200\0"
                                                              u8" 230400\0 250000\0 500000\0 1000000\0 2000000\0 3000000\0 4000000\0"))
            {
            }
        }
        else if (m_con_type == E_TCP)
        {
            // if (ImGui::InputText(u8"IP地址", &m_modbus.m_ip_addr, ImGuiInputTextFlags_CharsDecimal))
            //{
            // }
            // if (ImGui::InputInt(u8"端口号", &m_modbus.m_port_num, 1, 100))
            //{
            // }
        }
        ImGui::EndDisabled();
    }
    if (m_modbus.IsConnected())
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.00f, 0.40f, 0.40f, 1.00f));
        if (ImGui::Button(u8"断开", ImVec2(ImGui::GetContentRegionAvail().x - 5.0f, 80.0f)))
        {
            m_modbus.Disconnect();
            m_slave_id_index = -1;
        }
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::Button(u8"连接", ImVec2(ImGui::GetContentRegionAvail().x - 5.0f, 80.0f)))
        {
            if (m_con_type == E_TCP)
            {
                m_warning_msg = u8"暂不支持该协议！";
                m_connect_failed_warning = true;
            }
            else if (!m_modbus.Connect(m_vec_coms[mb_cfg->com_index], mb_cfg->baud_rate))
            {
                m_warning_msg = u8"可能存在以下原因：\n1. USB驱动未安装\n2. USB未连接到电脑\n3. 设备故障\n4. COM口已被其他软件打开\n5. 其他原因，请咨询技术人员";
                m_connect_failed_warning = true;
            }
            // save cfg
            g_cfg->SaveCfg();
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
    stbsp_sprintf(buf, u8"%s 选择从站地址", ICON_FA_CIRCLE_INFO);
    ImGui::Text(buf);
    if (custom_combo(m_slave_ids, u8"从站地址", buf, m_slave_id_index, [](int slave_id, size_t index, char *out_buf)
                     { stbsp_sprintf(out_buf, u8"%d", slave_id); }))
    {
        int active_slave_id = m_slave_ids[m_slave_id_index];
        m_modbus.SetActiveSlaveId(active_slave_id);
        m_modbus.QuerySystemSettings(m_sys_settings);
        m_modbus.QueryVersionInfo(m_version);
        m_modbus.QuerySystemStatus(m_status);
    }

    if (ImGui::CollapsingHeader(u8"硬件信息", ImGuiTreeNodeFlags_DefaultOpen))
    {
        stbsp_sprintf(buf, u8"%d", m_version.version_major);
        ImGui::InputText(u8"主版本", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
        stbsp_sprintf(buf, u8"%d", m_version.version_minor);
        ImGui::InputText(u8"次版本", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
        if(m_status.sys_state == 0)
        {
            stbsp_sprintf(buf, u8"正在初始化");
        }
        else if(m_status.sys_state == 1)
        {
            stbsp_sprintf(buf, u8"正常");
        }
        else if(m_status.sys_state == 2)
        {
            stbsp_sprintf(buf, u8"系统错误");
        }
        else
        {
            stbsp_sprintf(buf, u8"未知状态");
        }
        ImGui::InputText(u8"当前状态", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
        stbsp_sprintf(buf, u8"%d", m_status.error_code);
        ImGui::InputText(u8"错误码", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::CollapsingHeader(u8"参数设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button(u8"加载参数"))
        {
            if (m_modbus.QuerySystemSettings(m_sys_settings))
            {
                UI_INFO(u8"查询系统参数成功");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"保存参数"))
        {
            if (m_modbus.SaveSystemSettings(m_sys_settings))
            {
                UI_INFO(u8"保存系统参数成功");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"重启系统"))
        {
            if (m_modbus.RestartSystem())
            {
                UI_INFO(u8"重启系统命令发送成功");
            }
            else
            {
                UI_ERROR(u8"重启系统命令发送失败");
            }
        }

        bool enabled = m_sys_settings.task_mask & TASK_LOG_MASK;
        ImGui::SeparatorText(u8"设置启动任务");
        if (ImGui::Checkbox(u8"LOG任务", &enabled))
        {
            if (enabled)
            {
                m_sys_settings.task_mask |= TASK_LOG_MASK;
            }
            else
            {
                m_sys_settings.task_mask &= ~TASK_LOG_MASK;
            }
        }
        enabled = m_sys_settings.task_mask & TASK_AD_MASK;
        ImGui::SameLine();
        if (ImGui::Checkbox(u8"AD任务", &enabled))
        {
            if (enabled)
            {
                m_sys_settings.task_mask |= TASK_AD_MASK;
            }
            else
            {
                m_sys_settings.task_mask &= ~TASK_AD_MASK;
            }
        }
        enabled = m_sys_settings.task_mask & TASK_CAN_MASK;
        ImGui::SameLine();
        if (ImGui::Checkbox(u8"CAN任务", &enabled))
        {
            if (enabled)
            {
                m_sys_settings.task_mask |= TASK_CAN_MASK;
            }
            else
            {
                m_sys_settings.task_mask &= ~TASK_CAN_MASK;
            }
        }
        enabled = m_sys_settings.task_mask & TASK_HOMOTOR_MASK;
        ImGui::SameLine();
        if (ImGui::Checkbox(u8"HO任务", &enabled))
        {
            if (enabled)
            {
                m_sys_settings.task_mask |= TASK_HOMOTOR_MASK;
            }
            else
            {
                m_sys_settings.task_mask &= ~TASK_HOMOTOR_MASK;
            }
        }
        enabled = m_sys_settings.task_mask & TASK_LAMOTOR_MASK;
        ImGui::SameLine();
        if (ImGui::Checkbox(u8"LA任务", &enabled))
        {
            if (enabled)
            {
                m_sys_settings.task_mask |= TASK_LAMOTOR_MASK;
            }
            else
            {
                m_sys_settings.task_mask &= ~TASK_LAMOTOR_MASK;
            }
        }
        int slave_id = m_sys_settings.mb_slave_id;
        stbsp_sprintf(buf, u8"从站地址[1,10]##2");
        if (ImGui::InputInt(buf, &slave_id, 1, 2))
        {
            slave_id = std::clamp(slave_id, 1, EMB_MAX_SLAVE_ID_NUM);
            m_sys_settings.mb_slave_id = (uint8_t)slave_id;
        }
        // 采样参数设置
        if (ImGui::TreeNodeEx(u8"采样设置", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool is_write_adc_to_uart1 = m_sys_settings.write_adc_to_uart1 == 1;
            if (ImGui::Checkbox(u8"将ADC数据写入串口1", &is_write_adc_to_uart1))
            {
                m_sys_settings.write_adc_to_uart1 = is_write_adc_to_uart1 ? 1 : 0;
            }
            DrawHelpTipForLastItem(u8"设置后，保存，然后重启MCU");

            ImGui::SeparatorText(u8"通道使能");
            for (int i = 0; i < ADC_CHANNEL_NUM; i++)
            {
                stbsp_sprintf(buf, u8"CH%d", i + 1);
                bool enabled = (m_sys_settings.adc_config.ch_enable_mask & (1 << i)) != 0;
                if (i != 0)
                {
                    ImGui::SameLine();
                }
                if (ImGui::Checkbox(buf, &enabled))
                {
                    // 更新通道使能掩码
                    if (enabled)
                    {
                        m_sys_settings.adc_config.ch_enable_mask |= (1 << i);
                    }
                    else
                    {
                        m_sys_settings.adc_config.ch_enable_mask &= ~(1 << i);
                    }
                }
            }
            ImGui::SeparatorText(u8"其他参数");
            static int step = 1;
            static int step_fast = 10;
            // PGA增益
            if (ImGui::InputScalar(u8"PGA增益", ImGuiDataType_U8, &m_sys_settings.adc_config.pga_gain, &step, &step_fast, "%d"))
            {
            }
            // 滤波器FS值
            if (ImGui::InputScalar(u8"滤波器FS值", ImGuiDataType_U16, &m_sys_settings.adc_config.ch_filter_fs, &step, &step_fast, "%d"))
            {
            }

            ImGui::TreePop();
        }
    }
    if (ImGui::CollapsingHeader(u8"寄存器读写", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                       ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX;

        // shared table flags
        const float FRAME_HEIGHT = ImGui::GetFrameHeight();
        ImVec2 outer_size = ImVec2(0.0f, FRAME_HEIGHT * 15.0f);

        bool is_paused_query = m_modbus.IsPauseQuery();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(230 / 255.0f, 120 / 255.0f, 20 / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(230 / 255.0f, 120 / 255.0f, 20 / 255.0f, 1.0f));
        if (ImGui::Toggle(is_paused_query ? u8"定时查询已关闭" : u8"定时查询已开启", &is_paused_query, ImVec2(0.0f, 0.0f)))
        {
            m_modbus.SetPauseQuery(is_paused_query);
        }
        ImGui::PopStyleColor(2);
        if (!is_paused_query)
        {
            int query_ms = m_modbus.GetQueryTimeMs();
            if (ImGui::SliderInt(u8"ADC查询间隔(ms)", &query_ms, 1, 1000))
            {
                m_modbus.SetQueryTimeMs(query_ms);
            }
        }
    }
    ImGui::End();
}
