#include "UIYHGripper.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "UIConnectionSettings.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>

UIYHGripper::UIYHGripper(UIMainWindowBase *main_win, const char *title) : UIBaseWindow(main_win, title)
{
    // 从UIConnectionSettings对象获取ModbusProxy指针并赋值给m_modbus
    UIMainWindow *uimain_win = dynamic_cast<UIMainWindow *>(main_win);
    UIWindowManagerPtr win_mng = std::dynamic_pointer_cast<UIWindowManager>(uimain_win->GetWindowManager());
    m_modbus = win_mng->GetConnectionSettings()->GetModbusProxy();
    m_hypersen_proxy = win_mng->GetHypersenMonitor()->GetHypersenProxy();
    // 注册数据回调函数
    m_modbus->RegisterDataCallback(std::bind(&UIYHGripper::OnRegDataCB, this, std::placeholders::_1, std::placeholders::_2));
}

UIYHGripper::~UIYHGripper()
{
}

void UIYHGripper::Draw()
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
    ImGui::SeparatorText(u8"夹爪力控实验");
    ImGui::BeginDisabled(m_experiment_run_flag);
    ImGui::InputInt(u8"夹爪宽度(mm)", &m_gripper_width_mm);
    ImGui::InputInt(u8"等待时间(s)", &m_wait_time_sec);
    ImGui::InputInt(u8"电机1开始位置", &m_motor1_begin);
    ImGui::InputInt(u8"电机1结束位置", &m_motor1_end);
    ImGui::InputInt(u8"电机2（3）开始位置", &m_motor2_begin);
    ImGui::InputInt(u8"电机2（3）结束位置", &m_motor2_end);
    ImGui::EndDisabled();
    if (!m_experiment_run_flag)
    {
        if (ImGui::Button(u8"开始实验", ImVec2(-1, 0)))
        {
            m_experiment_run_flag = true;
            m_experiment_thread = std::thread(&UIYHGripper::ExperimentThreadFunc, this);
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
        if (ImGui::BeginTable("ExperimentDataTable", 18, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // 冻结第一行
            ImGui::TableSetupColumn(u8"序号", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn(u8"夹爪宽度(mm)", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(u8"电机1位置", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(u8"电机2位置", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(u8"电机3位置", ImGuiTableColumnFlags_WidthStretch);
            for (int i = 0; i < ADC_CHANNEL_NUM; ++i)
            {
                stbsp_sprintf(buf, u8"ADC%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            for (int i = 0; i < HYPERSEN_SENSOR_DOF; ++i)
            {
                stbsp_sprintf(buf, u8"F%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            ImGui::TableHeadersRow();

            std::vector<ST_ExperimentData> experiment_data;
            {
                std::lock_guard<std::mutex> lock(m_experiment_data_lock);
                experiment_data = m_experiment_data;
            }
            // 使用ImGuiListClipper来优化大数据量的渲染
            ImGuiListClipper clipper;
            clipper.Begin((int)experiment_data.size());
            while (clipper.Step())
            {
                for (size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    const ST_ExperimentData &data = experiment_data[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", i + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", data.gripper_width_mm);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", data.lamotor_pos[0]);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", data.lamotor_pos[1]);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", data.lamotor_pos[2]);
                    for (int j = 0; j < ADC_CHANNEL_NUM; ++j)
                    {
                        ImGui::TableSetColumnIndex(5 + j);
                        ImGui::Text("%.3f", data.adc_data[j]);
                    }
                    for (int j = 0; j < HYPERSEN_SENSOR_DOF; ++j)
                    {
                        ImGui::TableSetColumnIndex(5 + ADC_CHANNEL_NUM + j);
                        ImGui::Text("%.3f", data.force_data[j]);
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void UIYHGripper::OnRegDataCB(int slave_id, const adc_info_t *data)
{
    // 更新当前输入寄存器数据
    {
        std::lock_guard<std::mutex> lock(m_cur_adc_lock);
        m_slave_id = slave_id;
        m_cur_adc = *data;
    }
}

void UIYHGripper::ExperimentThreadFunc()
{
    lamotor_cmds_t lamotor_cmds = {0};
    m_modbus->QueryLamotorCmds(lamotor_cmds);
    // 启动电机
    lamotor_cmds.enable = 1;
    m_modbus->SetLamotorCmds(lamotor_cmds);
    // 清空实验数据
    {
        std::lock_guard<std::mutex> lock(m_experiment_data_lock);
        m_experiment_data.clear();
    }
    // 电机运动步长
    int step = 100;
    for (int m1 = m_motor1_begin; m1 >= m_motor1_end && m_experiment_run_flag; m1 -= step)
    {
        for (int m23 = m_motor2_begin; m23 <= m_motor2_end && m_experiment_run_flag; m23 += step)
        {
            LOG_INFO("Moving to: m1=%d, m23=%d", m1, m23);
            // 设置电机位置
            lamotor_cmds.cmd[0].lamotor_id = 1;
            lamotor_cmds.cmd[0].lamotor_target_pos = m1;
            lamotor_cmds.cmd[0].lamotor_mode = 0; // enable

            lamotor_cmds.cmd[1].lamotor_id = 2;
            lamotor_cmds.cmd[1].lamotor_target_pos = m23;
            lamotor_cmds.cmd[1].lamotor_mode = 0; // enable

            lamotor_cmds.cmd[2].lamotor_id = 3;
            lamotor_cmds.cmd[2].lamotor_target_pos = m23;
            lamotor_cmds.cmd[2].lamotor_mode = 0;

            m_modbus->SetLamotorCmds(lamotor_cmds);
            // 等待运动完成
            for (int t = 0; t < m_wait_time_sec && m_experiment_run_flag; ++t)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            // 获取当前实验数据
            ST_ExperimentData data;
            data.gripper_width_mm = m_gripper_width_mm;
            data.lamotor_pos[0] = m1;
            data.lamotor_pos[1] = m23;
            data.lamotor_pos[2] = m23;
            // 获取应变片数据
            for (int i = 0; i < ADC_CHANNEL_NUM; ++i)
            {
                data.adc_data[i] = m_cur_adc.channel_data[i] / 10.0f;
            }
            // 获取传感器数据
            ST_HypersenSensorData sensor_data = m_hypersen_proxy->GetCurrentData();
            for (int i = 0; i < HYPERSEN_SENSOR_DOF; ++i)
            {
                data.force_data[i] = sensor_data.data[i];
            }
            // 保存实验数据
            {
                std::lock_guard<std::mutex> lock(m_experiment_data_lock);
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
            time_t now = time(nullptr);
            struct tm *tm_info = localtime(&now);
            char filename[64];
            stbsp_snprintf(filename, sizeof(filename), "experiment_data_%dmm_%d%02d%02d_%02d%02d%02d.csv",
                           m_gripper_width_mm, tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                           tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
            LOG_INFO("Saving experiment data to: %s", filename);
            // 打开文件
            std::ofstream ofs(filename);
            if (!ofs.is_open())
            {
                LOG_ERROR("Failed to open file: %s", filename);
                return;
            }
            // 写入CSV头
            ofs << "GripperWidth,Motor1Pos,Motor2Pos,Motor3Pos,";
            for (int i = 0; i < ADC_CHANNEL_NUM; ++i)
            {
                ofs << "ADC" << i << ",";
            }
            for (int i = 0; i < HYPERSEN_SENSOR_DOF; ++i)
            {
                ofs << "Force" << i << ",";
            }
            ofs << "\n";
            // 写入实验数据
            ofs.precision(6);
            for (const auto &data : m_experiment_data)
            {
                ofs << data.gripper_width_mm << ",";
                ofs << data.lamotor_pos[0] << "," << data.lamotor_pos[1] << "," << data.lamotor_pos[2] << ",";
                for (int i = 0; i < ADC_CHANNEL_NUM; ++i)
                {
                    ofs << data.adc_data[i] << ",";
                }
                for (int i = 0; i < HYPERSEN_SENSOR_DOF; ++i)
                {
                    ofs << data.force_data[i] << ",";
                }
                ofs << "\n";
            }
        }
    }
    LOG_INFO("Experiment thread exiting");
}
