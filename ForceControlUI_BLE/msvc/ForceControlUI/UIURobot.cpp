#include "UIURobot.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"

#define _PI 3.14159265358979323846f

UIURobot::UIURobot(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
{
    strcpy(m_hostname, "192.168.12.10"); // 默认IP地址
}

UIURobot::~UIURobot()
{
}

void UIURobot::Draw()
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
    char buf[256] = {0};

    if (ImGui::CollapsingHeader(u8"连接机器人", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputText(u8"机器人IP", m_hostname, sizeof(m_hostname));
        ImGui::SameLine();

        if (ImGui::Button(m_ur.IsConnected() ? u8"断开" : u8"连接"))
        {
            if (!m_ur.IsConnected())
            {
                if (m_ur.Connect(m_hostname))
                {
                    UI_INFO(u8"连接%s成功！", m_hostname);
                    if (m_ur.ConnectGripper(m_hostname))
                    {
                        UI_INFO(u8"连接夹爪成功！");
                    }
                }
            }
            else
            {
                m_ur.Disconnect();
                m_ur.DisconnectGripper();
                UI_INFO(u8"断开连接成功！");
            }
        }
        if (m_ur.IsConnected())
        {
            ImGui::SameLine();
            if(ImGui::Button(u8"打开电源"))
            {
                if (m_ur.PowerOnRobot())
                {
                    UI_INFO(u8"机器人电源打开成功！");
                }
                else
                {
                    UI_ERROR(u8"机器人电源打开失败！");
                }
            }
            ImGui::SameLine();
            if(ImGui::Button(u8"关闭电源"))
            {
                if (m_ur.PowerOffRobot())
                {
                    UI_INFO(u8"机器人电源关闭成功！");
                }
                else
                {
                    UI_ERROR(u8"机器人电源关闭失败！");
                }
            }
            ImGui::SameLine();
            if(ImGui::Button(u8"关机"))
            {
                if (m_ur.ShutdownRobot())
                {
                    UI_INFO(u8"机器人关机成功！");
                }
                else
                {
                    UI_ERROR(u8"机器人关机失败！");
                }
            }
        }
    }
    if (ImGui::CollapsingHeader(u8"机器人状态", ImGuiTreeNodeFlags_DefaultOpen))
    {
        m_ur_data = m_ur.GetURDataFrame();

        ImGui::InputDouble(u8"Runing Time", &m_ur_data.running_time, 0, 0, "%.0f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputInt(u8"Robot Mode", &m_ur_data.robot_mode, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::InputInt(u8"Safety Mode", &m_ur_data.safety_mode, 0, 0, ImGuiInputTextFlags_ReadOnly);
        float joints[6] = {0};
        float tcp[6] = {0};
        for (size_t i = 0; i < 6; i++)
        {
            joints[i] = static_cast<float>(m_ur_data.joint_info[i]);
            tcp[i] = static_cast<float>(m_ur_data.cartesian_info[i]);
        }
        ImGui::InputFloat3(u8"Joint Value[J1-J3](单位rad)", joints, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat3(u8"Joint Value[J4-J6](单位rad)", joints + 3, "%.3f", ImGuiInputTextFlags_ReadOnly);

        ImGui::InputFloat3(u8"TCP Value[x,y,z](单位m)", tcp, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat3(u8"TCP Value[rx,ry,rz](单位rad)", tcp + 3, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }

    if (ImGui::CollapsingHeader(u8"控制机器人", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static float joints[6] = {0};
        static float tcp[6] = {0};

        ImGui::SliderFloat(u8"UR运动速度", &m_ur_move_speed, 0.01f, 0.50f, "%.2f");
        ImGui::SliderFloat(u8"UR运动加速度", &m_ur_move_accel, 0.05f, 2.00f, "%.2f");

        if (ImGui::Button(u8"回零", ImVec2(-1, 0)))
        {
            m_ur.MoveHome(m_ur_move_speed, m_ur_move_accel);
        }


        ImGui::SliderFloat3(u8"Joint [J1-J3](单位rad)", joints, -2 * _PI, 2 * _PI, "%.3f");
        ImGui::SliderFloat3(u8"Joint [J4-J6](单位rad)", joints + 3, -2 * _PI, 2 * _PI, "%.3f");

        if (ImGui::Button("MoveJ", ImVec2(-1, 0)))
        {
            std::vector<double> movej_pos(joints, joints + 6);
            m_ur.MoveJ(movej_pos, m_ur_move_speed, m_ur_move_accel);
        }

        ImGui::InputFloat3(u8"TCP [x,y,z](单位m)", tcp, "%.3f");
        ImGui::SliderFloat3(u8"TCP [rx,ry,rz](单位rad)", tcp + 3, 0, 2 * _PI, "%.3f");

        if (ImGui::Button("MoveL", ImVec2(-1, 0)))
        {
            std::vector<double> movel_pos(tcp, tcp + 6);
            m_ur.MoveL(movel_pos, m_ur_move_speed, m_ur_move_accel);
        }
    }

    if (ImGui::CollapsingHeader(u8"Robotiq夹爪状态", ImGuiTreeNodeFlags_DefaultOpen))
    {
        m_gripper_data = m_ur.GetGripperDataFrame();
        ImGui::Checkbox(u8"夹爪激活状态", &m_gripper_data.is_active);
        ImGui::InputFloat(u8"夹爪当前位置", &m_gripper_data.position, 0, 0, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat(u8"夹爪闭合位置", &m_gripper_data.closed_position, 0, 0, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat(u8"夹爪打开位置", &m_gripper_data.open_position, 0, 0, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputInt(u8"夹爪状态", &m_gripper_data.object_status, 0, 0, ImGuiInputTextFlags_ReadOnly);
        ImGui::InputInt(u8"夹爪故障码", &m_gripper_data.fault_code, 0, 0, ImGuiInputTextFlags_ReadOnly);
    }

    if (ImGui::CollapsingHeader(u8"夹爪控制", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static float gripper_position = 0.0f;
        static float gripper_speed = 0.5f;
        static float gripper_force = 0.5f;

        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(u8"打开夹爪", ImVec2(w / 2 - 5, 0)))
        {
            m_ur.OpenGripper();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"关闭夹爪", ImVec2(w / 2 - 5, 0)))
        {
            m_ur.CloseGripper();
        }

        ImGui::SliderFloat(u8"夹爪位置", &gripper_position, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat(u8"夹爪速度", &gripper_speed, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat(u8"夹爪力度", &gripper_force, 0.0f, 1.0f, "%.3f");

        if (ImGui::Button(u8"移动夹爪", ImVec2(-1, 0)))
        {
            m_ur.MoveGripper(gripper_position, gripper_speed, gripper_force, false);
        }
    }

    ImGui::End();
}
