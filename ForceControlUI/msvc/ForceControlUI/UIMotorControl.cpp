#include "UIMotorControl.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "UIConnectionSettings.h"
#include "imgui/imgui.h"
#include "stb/stb_sprintf.h"

UIMotorControl::UIMotorControl(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
{
    // 从UIConnectionSettings对象获取ModbusProxy指针并赋值给m_modbus
    UIMainWindow *uimain_win = dynamic_cast<UIMainWindow *>(main_win);
    UIWindowManagerPtr win_mng = std::dynamic_pointer_cast<UIWindowManager>(uimain_win->GetWindowManager());
    m_modbus = win_mng->GetConnectionSettings()->GetModbusProxy();

    // 注册数据回调函数
    // m_modbus->RegisterDataCallback(std::bind(&UIMotorControl::OnRegDataCB, this, std::placeholders::_1, std::placeholders::_2));
}

UIMotorControl::~UIMotorControl()
{
}

void UIMotorControl::Draw()
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

    uint8_t id_range[2] = {1, 254};
    if (ImGui::CollapsingHeader(u8"因时电机控制", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(!m_modbus->IsConnected());
        if (ImGui::Button(u8"刷新数据##1"))
        {
            m_modbus->QueryLamotorCmds(m_lamotor_cmds);
            m_modbus->QueryLamotorInfo(m_lamotor_info);
        }
        ImGui::SameLine();
        if (m_lamotor_cmds.enable)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(u8"当前状态: 运行中");
            ImGui::SameLine();
            if (ImGui::Button(u8"停止##1"))
            {
                m_lamotor_cmds.enable = 0; // 清除使能标志位
                m_modbus->SetLamotorCmds(m_lamotor_cmds);
            }
        }
        else
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(u8"当前状态: 停止");
            ImGui::SameLine();
            if (ImGui::Button(u8"启动##1"))
            {
                m_lamotor_cmds.enable = 1; // 设置使能标志位
                m_modbus->SetLamotorCmds(m_lamotor_cmds);
            }
        }
        ImGui::EndDisabled();

        if (ImGui::SliderScalar(u8"电机ID##1", ImGuiDataType_U8, &m_lamotor_cmds.cmd[0].lamotor_id, (void *)&id_range[0], (void *)&id_range[1], "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            m_modbus->SetLamotorCmds(m_lamotor_cmds);
        }
        int16_t range[2] = {0, 2000};
        if (ImGui::SliderScalar(u8"因时电机位置控制", ImGuiDataType_S16, &m_lamotor_cmds.cmd[0].lamotor_target_pos, (void *)&range[0], (void *)&range[1], "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            m_modbus->SetLamotorCmds(m_lamotor_cmds);
        }

        ImGui::SeparatorText(u8"电机实时信息##1");
        ImGui::InputScalar(u8"电机反馈ID##1", ImGuiDataType_S8, &m_lamotor_info[0].motor_id, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        // ImGui::InputScalar(u8"电机温度(°C)##1", ImGuiDataType_S8, &m_lamotor_info[0].temp, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);

        ImGui::InputScalar(u8"目标位置##1", ImGuiDataType_S16, &m_lamotor_info[0].target_pos, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        // ImGui::InputScalar(u8"当前位置##1", ImGuiDataType_S16, &m_lamotor_info[0].current_pos, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);

        // ImGui::InputScalar(u8"电机电流##1", ImGuiDataType_U16, &m_lamotor_info[0].current, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        // ImGui::InputScalar(u8"电机力##1", ImGuiDataType_S16, &m_lamotor_info[0].force, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);

        // ImGui::InputScalar(u8"电机错误码##1", ImGuiDataType_S8, &m_lamotor_info[0].error, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::CollapsingHeader(u8"中空电机控制", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(!m_modbus->IsConnected());
        if (ImGui::Button(u8"刷新数据##2"))
        {
            m_modbus->QueryHomotorCmds(m_homotor_cmds);
            m_modbus->QueryHomotorInfo(m_homotor_info);
        }
        ImGui::SameLine();
        if (m_homotor_cmds.enable)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(u8"当前状态: 运行中");
            ImGui::SameLine();
            if (ImGui::Button(u8"停止##2"))
            {
                m_homotor_cmds.enable = 0; // 清除使能标志位
                m_modbus->SetHomotorCmds(m_homotor_cmds);
            }
        }
        else
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(u8"当前状态: 停止");
            ImGui::SameLine();
            if (ImGui::Button(u8"启动##2"))
            {
                m_homotor_cmds.enable = 1; // 设置使能标志位
                m_modbus->SetHomotorCmds(m_homotor_cmds);
            }
        }
        ImGui::EndDisabled();

        if (ImGui::SliderScalar(u8"电机ID##2", ImGuiDataType_U8, &m_homotor_cmds.cmd[0].homotor_id, (void *)&id_range[0], (void *)&id_range[1], "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }
        int homotor_mode = m_homotor_cmds.cmd[0].homotor_mode;
        if (ImGui::Combo(u8"控制模式##2", &homotor_mode, u8"位置\0速度\0扭矩\0\0"))
        {
            m_homotor_cmds.cmd[0].homotor_mode = (uint8_t)homotor_mode;
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }

        float range[2] = {-355, 355};
        if (ImGui::SliderFloat(u8"位置控制(度)##2", &m_homotor_cmds.cmd[0].homotor_pos_deg, range[0], range[1], "%.2f"))
        {
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }
        if (ImGui::InputFloat(u8"速度控制(弧度/秒)##2", &m_homotor_cmds.cmd[0].homotor_speed_rad, 0.1f, 1.0f, "%.2f"))
        {
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }

        if (ImGui::InputInt(u8"位置比例增益P##2", &m_homotor_cmds.cmd[0].homotor_pos_p, 1, 100))
        {
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }
        if (ImGui::InputInt(u8"位置微分增益D##2", &m_homotor_cmds.cmd[0].homotor_pos_d, 1, 100))
        {
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }

        if (ImGui::InputFloat(u8"电流控制(A)##2", &m_homotor_cmds.cmd[0].homotor_current, 0.1f, 1.0f, "%.2f"))
        {
            m_modbus->SetHomotorCmds(m_homotor_cmds);
        }

        ImGui::SeparatorText(u8"电机实时信息##2");
        ImGui::InputScalar(u8"电机反馈ID##2", ImGuiDataType_S8, &m_homotor_info[0].motor_id, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputInt(u8"电机位置(度)##2", &m_homotor_info[0].current_pos, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::InputScalar(u8"电机速度(弧度/秒)##2", ImGuiDataType_S16, &m_homotor_info[0].current_speed, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputScalar(u8"电机扭矩(A)##2", ImGuiDataType_S16, &m_homotor_info[0].current_torque, NULL, NULL, "%d", ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::End();
}
