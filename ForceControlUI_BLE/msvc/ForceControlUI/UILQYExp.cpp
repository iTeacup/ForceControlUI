#include "UILQYExp.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "UIConnectionSettings.h"
#include "UIOptiTrack.h"
#include "UIURobot.h"
#include "UIApplication.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>

UILQYExp::UILQYExp(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
{
    // 从UIConnectionSettings对象获取ModbusProxy指针并赋值给m_modbus
    UIMainWindow *uimain_win = dynamic_cast<UIMainWindow *>(main_win);
    UIWindowManagerPtr win_mng = std::dynamic_pointer_cast<UIWindowManager>(uimain_win->GetWindowManager());
    m_modbus = win_mng->GetConnectionSettings()->GetModbusProxy();
    m_ur_manager = win_mng->GetURobot()->GetURManager();
    m_opti_track = win_mng->GetOptiTrack();
    // 获取CPS API指针
    m_cps_api = g_app.GetCPSApi();
    // 注册数据回调函数
    m_modbus->RegisterDataCallback(std::bind(&UILQYExp::OnRegDataCB, this, std::placeholders::_1, std::placeholders::_2));
}

UILQYExp::~UILQYExp()
{
}

void UILQYExp::Draw()
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
    ImGui::SeparatorText(u8"力控实验");
    ImGui::BeginDisabled(m_experiment_run_flag);
    ImGui::InputInt(u8"电机步进量", &m_lamotor_step);
    ImGui::InputInt(u8"等待时间(s)", &m_wait_time_sec);
    ImGui::EndDisabled();

    if (ImGui::CollapsingHeader(u8"力传感监控", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button(u8"力传感器清零", ImVec2(-1, 0)))
        {
            ReqSensorResetZero();
        }
        ST_SRISensorData cur_force;
        {
            std::lock_guard<std::mutex> lk(m_cur_force_lock);
            cur_force = m_cur_force;
        }
        ImGui::InputFloat3(u8"Fx,Fy,Fz", (float *)&cur_force, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat3(u8"Mx,My,Mz", (float *)&cur_force.mx_main, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }
    if (ImGui::CollapsingHeader(u8"ADC监控", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float adc_mv[2] = {0};
        {
            std::lock_guard<std::mutex> lk(m_cur_adc_lock);
            adc_mv[0] = m_cur_adc.channel_data[0] * 0.1f;
            adc_mv[1] = m_cur_adc.channel_data[1] * 0.1f;
        }
        ImGui::InputFloat2(u8"CH1,CH2(0.1mV)", adc_mv, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }

    if (!m_experiment_run_flag)
    {
        if (ImGui::Button(u8"开始实验", ImVec2(-1, 0)))
        {
            m_experiment_run_flag = true;
            m_experiment_thread = std::thread(&UILQYExp::ExperimentThreadFunc, this);
        }
    }
    else
    {
        if (ImGui::Button(u8"停止实验", ImVec2(-1, 0)))
        {
            m_experiment_run_flag = false;
            if (m_experiment_thread.joinable())
            {
                m_experiment_thread.join();
            }
        }
    }
    if (ImGui::CollapsingHeader(u8"实验数据", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                       ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX;
        if (ImGui::BeginTable("ExperimentDataTable", 16, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // 冻结第一行
            ImGui::TableSetupColumn(u8"序号", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn(u8"电机位置", ImGuiTableColumnFlags_WidthStretch);
            for (int i = 0; i < 2; ++i)
            {
                stbsp_sprintf(buf, u8"ADC%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            for (int i = 0; i < 6; ++i)
            {
                stbsp_sprintf(buf, u8"F%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            for (int i = 0; i < 6; ++i)
            {
                stbsp_sprintf(buf, u8"M%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            ImGui::TableHeadersRow();

            std::vector<ST_LQYExpData> experiment_data;
            {
                std::lock_guard<std::mutex> lock(m_experiment_data_lock);
                experiment_data = m_experiment_data;
            }
            // 使用ImGuiListClipper来优化大数据量的渲染
            ImGuiListClipper clipper;
            clipper.Begin((int)experiment_data.size());
            while (clipper.Step())
            {
                for (size_t row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    stbsp_sprintf(buf, "%zu", row + 1);
                    ImGui::TextUnformatted(buf);

                    ImGui::TableSetColumnIndex(1);
                    stbsp_sprintf(buf, "%d", experiment_data[row].lamotor_pos);
                    ImGui::TextUnformatted(buf);

                    for (int col = 0; col < 2; ++col)
                    {
                        ImGui::TableSetColumnIndex(2 + col);
                        stbsp_sprintf(buf, "%.3f", experiment_data[row].adc_data[col]);
                        ImGui::TextUnformatted(buf);
                    }
                    for (int col = 0; col < 6; ++col)
                    {
                        ImGui::TableSetColumnIndex(4 + col);
                        stbsp_sprintf(buf, "%.3f", (float *)(&experiment_data[row].force_data) + col);
                        ImGui::TextUnformatted(buf);
                    }
                    for (int col = 0; col < 6; ++col)
                    {
                        ImGui::TableSetColumnIndex(10 + col);
                        stbsp_sprintf(buf, "%d(%.3f,%.3f,%.3f)", experiment_data[row].markers[col].ID,
                                      experiment_data[row].markers[col].XYZ[0],
                                      experiment_data[row].markers[col].XYZ[1],
                                      experiment_data[row].markers[col].XYZ[2]);
                        ImGui::TextUnformatted(buf);
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void UILQYExp::OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char *data, uint32_t msg_len)
{
    switch (msg_type)
    {
    case MSG_SRI_SENSOR_DATA:
        if (msg_len == sizeof(ST_SRISensorData))
        {
            std::lock_guard<std::mutex> lk(m_cur_force_lock);
            m_cur_force = *(ST_SRISensorData *)data;
        }
        break;
    default:
        break;
    }
}

void UILQYExp::OnRegDataCB(int slave_id, const adc_info_t *data)
{
    std::lock_guard<std::mutex> lk(m_cur_adc_lock);
    m_slave_id = slave_id;
    m_cur_adc = *data;
}

void UILQYExp::ExperimentThreadFunc()
{
    if (!m_modbus || !m_ur_manager)
    {
        m_experiment_run_flag = false;
        return;
    }
    lamotor_cmds_t lamotor_cmds = {0};
    m_modbus->QueryLamotorCmds(lamotor_cmds);
    // 启动电机
    lamotor_cmds.enable = 1;
    m_modbus->SetLamotorCmds(lamotor_cmds);
    // 清空之前的实验数据
    {
        std::lock_guard<std::mutex> lk(m_experiment_data_lock);
        m_experiment_data.clear();
    }
    // UR机器人关节列表
    std::vector<std::vector<double>> ur_joint_positions;
    // TODO: 根据实验需要从文件加载UR机器人关节位置

    // 外层for循环，每次步进电机
    for (int motor_pos = 0; motor_pos < 2000; motor_pos += m_lamotor_step)
    {
        // 内层for循环，每次移动一下UR机器人
        for (const auto &joints : ur_joint_positions)
        {
            if (!m_experiment_run_flag)
            {
                break;
            }
            // 移动UR机器人
            m_ur_manager->MoveJ(joints, 0.5, 0.5);
            // 设置电机位置
            lamotor_cmds.cmd[0].lamotor_id = 1;
            lamotor_cmds.cmd[0].lamotor_mode = 0; // enable
            lamotor_cmds.cmd[0].lamotor_target_pos = motor_pos;
            m_modbus->SetLamotorCmds(lamotor_cmds);
            // 等待一段时间，确保电机到达位置
            for (int i = 0; i < m_wait_time_sec; ++i)
            {
                if (!m_experiment_run_flag)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            // 获取当前实验数据
            ST_LQYExpData data;
            data.lamotor_pos = motor_pos;
            {
                std::lock_guard<std::mutex> lk(m_cur_adc_lock);
                data.adc_data[0] = m_cur_adc.channel_data[0] * 0.1f;
                data.adc_data[1] = m_cur_adc.channel_data[1] * 0.1f;
            }
            {
                std::lock_guard<std::mutex> lk(m_cur_force_lock);
                data.force_data = m_cur_force;
            }
            ST_OptMarker_List markers;
            if (m_opti_track)
            {
                m_opti_track->GetSelectedMarkerData(markers);
                int copy_num = min(markers.marker_num, 6);
                for (int i = 0; i < copy_num; ++i)
                {
                    data.markers[i] = markers.markers[i];
                }
            }
            {
                std::lock_guard<std::mutex> lk(m_experiment_data_lock);
                m_experiment_data.push_back(data);
            }
        }
    }
    // 将实验数据存储到CSV
    {
        std::lock_guard<std::mutex> lock(m_experiment_data_lock);
        if (!m_experiment_data.empty())
        {
            // 按日期时间和夹爪宽度格式化文件名
            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);
            char filename[128];
            std::strftime(filename, sizeof(filename), "LQYExp_%Y%m%d_%H%M%S.csv", &tm);
            LOG_INFO("Saving experiment data to %s", filename);
            // 打开文件
            std::ofstream ofs(filename);
            if (ofs.is_open())
            {
                // 写入表头
                ofs << "序号,电机位置,ADC1(mV),ADC2(mV),Fx,Fy,Fz,Mx,My,Mz";
                for (int i = 1; i <= 6; ++i)
                {
                    ofs << ",Marker" << i << "_ID,Marker" << i << "_X,Marker" << i << "_Y,Marker" << i << "_Z";
                }
                ofs << "\n";
                // 写入数据
                for (size_t i = 0; i < m_experiment_data.size(); ++i)
                {
                    const auto &data = m_experiment_data[i];
                    ofs << (i + 1) << "," << data.lamotor_pos << ",";
                    ofs << std::fixed << std::setprecision(3) << data.adc_data[0] << "," << data.adc_data[1] << ",";
                    ofs << std::fixed << std::setprecision(3)
                        << data.force_data.fx_main << "," << data.force_data.fy_main << "," << data.force_data.fz_main << ","
                        << data.force_data.mx_main << "," << data.force_data.my_main << "," << data.force_data.mz_main;
                    for (int j = 0; j < 6; ++j)
                    {
                        ofs << "," << data.markers[j].ID
                            << "," << std::fixed << std::setprecision(3) << data.markers[j].XYZ[0]
                            << "," << std::fixed << std::setprecision(3) << data.markers[j].XYZ[1]
                            << "," << std::fixed << std::setprecision(3) << data.markers[j].XYZ[2];
                    }
                    ofs << "\n";
                }
                ofs.close();
                LOG_INFO("Experiment data saved to %s", filename);
            }
            else
            {
                LOG_ERROR("Failed to open file %s for writing", filename);
            }
        }
    }
    LOG_INFO("Experiment thread exiting");
}

void UILQYExp::ReqSensorResetZero()
{
    if (m_cps_api)
    {
        m_cps_api->SendAPPMsg(SRI_SENSOR_DEV_ID, MSG_REQ_SENSOR_RESET_ZERO, nullptr, 0);
    }
}
