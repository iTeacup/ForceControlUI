#include "UIBeamCalibration.h"
#include "UIMainWindow.h"
#include "UIWindowManager.h"
#include "UIConnectionSettings.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"
#include "Logger.h"
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include "UICfgParser.h"
#include "UIUtils.h"
#include "portable-file-dialogs.h"  // 单头文件

#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

UIBeamCalibration::UIBeamCalibration(UIMainWindowBase* main_win, const char* title) : UIBaseWindow(main_win, title)
{
    // 从UIConnectionSettings对象获取ModbusProxy指针并赋值给m_modbus
    UIMainWindow* uimain_win = dynamic_cast<UIMainWindow*>(main_win);
    UIWindowManagerPtr win_mng = std::dynamic_pointer_cast<UIWindowManager>(uimain_win->GetWindowManager());
    //m_modbus = win_mng->GetConnectionSettings()->GetModbusProxy();
    //m_hypersen_proxy = win_mng->GetHypersenMonitor()->GetHypersenProxy();
    //// 注册数据回调函数
    //m_modbus->RegisterDataCallback(std::bind(&UIBeamCalibration::OnRegDataCB, this, std::placeholders::_1, std::placeholders::_2));

    InitBleDevices();
    // 使用 ImGuiTextBuffer 行偏移：首元素为 0
    m_display_line_offsets.clear();
    m_display_line_offsets.push_back(0);

    strcpy(m_hostname, "192.168.12.10"); // 默认IP地址

}

UIBeamCalibration::~UIBeamCalibration()
{
    m_calibration_run_flag = false;
    m_adjust_finish_flag = true;
    if (m_calibration_thread.joinable())
    {
        m_calibration_thread.join();
    }
    StopAllBleDevices();
}

// ========== 静态工具方法实现 ==========

std::string UIBeamCalibration::RemoveNewlines(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (char c : s) {
        if (c != '\n' && c != '\r') {
            result += c;
        }
    }
    return result;
}

void UIBeamCalibration::ParseFloatArray(const std::string& input) {
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
        for (int i = 0; i < result.size(); i++)
        {
            m_sensors[i] = result[i];
        }
    }

}

void UIBeamCalibration::InitBleDevices()
{
    for (size_t i = 0; i < m_ble_devices.size(); ++i)
    {
        SBleBeamDevice& device = m_ble_devices[i];
        device.id = static_cast<uint32_t>(i + 1);
        stbsp_sprintf(device.display_name, u8"BLE设备%u", device.id);
        stbsp_sprintf(device.device_name, "nRF52840_%02u", device.id);
        stbsp_sprintf(device.service_uuid, "19B1%02u00-E8F2-537E-4F6C-D104768A1214", device.id);
        strcpy(device.characteristic_uuid, "19B10002-E8F2-537E-4F6C-D104768A1214");
        device.last_packet_time = std::chrono::steady_clock::now();
    }
}

bool UIBeamCalibration::IsBleBusyOrConnected(const SBleBeamDevice& device) const
{
    std::lock_guard<std::mutex> lock(device.state_lock);
    return device.state == EBleState::Scanning ||
        device.state == EBleState::Connecting ||
        device.state == EBleState::Connected ||
        device.state == EBleState::Disconnecting;
}

bool UIBeamCalibration::IsBleConnected(const SBleBeamDevice& device) const
{
    std::lock_guard<std::mutex> lock(device.state_lock);
    return device.state == EBleState::Connected;
}

void UIBeamCalibration::SetBleState(SBleBeamDevice* device, EBleState state, const std::string& message)
{
    if (device == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(device->state_lock);
    device->state = state;
    device->state_msg = message;
}

void UIBeamCalibration::StartBleConnect(SBleBeamDevice* device)
{
    if (device == nullptr)
    {
        return;
    }

    StopBleConnect(device);
    device->stop_ble = false;
    device->last_packet_time = std::chrono::steady_clock::now();
    device->packet_count = 0;
    device->last_packet.clear();
    device->ble_thread = std::thread(&UIBeamCalibration::BleThreadFunc, this, device);
}

void UIBeamCalibration::StopBleConnect(SBleBeamDevice* device)
{
    if (device == nullptr)
    {
        return;
    }

    device->stop_ble = true;
    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        if (device->state == EBleState::Scanning || device->state == EBleState::Connecting || device->state == EBleState::Connected)
        {
            device->state = EBleState::Disconnecting;
            device->state_msg = u8"正在断开 BLE...";
        }
    }

    if (device->ble_thread.joinable())
    {
        device->ble_thread.join();
    }
    SetBleState(device, EBleState::Disconnected, u8"未连接");
}

void UIBeamCalibration::StopAllBleDevices()
{
    for (auto& device : m_ble_devices)
    {
        StopBleConnect(&device);
    }
}

bool UIBeamCalibration::ParseBlePacket(const std::string& packet, ST_BeamBleData& data)
{
    std::stringstream ss(packet);
    std::string token;
    int index = 0;

    while (std::getline(ss, token, ',') && index < CALIBRATION_STRAIN_CHANNEL_NUM / BEAM_NUM)
    {
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        }), token.end());

        if (token.empty())
        {
            return false;
        }

        try
        {
            data.channel_mv[index++] = std::stof(token);
        }
        catch (...)
        {
            return false;
        }
    }

    return index == CALIBRATION_STRAIN_CHANNEL_NUM / BEAM_NUM;
}

void UIBeamCalibration::OnBlePacketReceived(SBleBeamDevice* device, const std::string& packet)
{
    if (device == nullptr)
    {
        return;
    }

    ST_BeamBleData data;
    if (!ParseBlePacket(packet, data))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(device->data_lock);
        device->current_data = data;
        device->last_packet_time = std::chrono::steady_clock::now();
    }

    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        device->last_packet = packet;
        ++device->packet_count;
    }
}

void UIBeamCalibration::CopyBleSensorData(float sensors[CALIBRATION_STRAIN_CHANNEL_NUM])
{
    for (size_t device_index = 0; device_index < m_ble_devices.size(); ++device_index)
    {
        ST_BeamBleData data;
        {
            std::lock_guard<std::mutex> lock(m_ble_devices[device_index].data_lock);
            data = m_ble_devices[device_index].current_data;
        }

        for (int channel = 0; channel < CALIBRATION_STRAIN_CHANNEL_NUM / BEAM_NUM; ++channel)
        {
            sensors[device_index * (CALIBRATION_STRAIN_CHANNEL_NUM / BEAM_NUM) + channel] = data.channel_mv[channel];
        }
    }
}

void UIBeamCalibration::BleThreadFunc(SBleBeamDevice* device)
{
    if (device == nullptr)
    {
        return;
    }

    try
    {
        init_apartment(apartment_type::multi_threaded);

        guid service_uuid{ std::string_view(device->service_uuid) };
        guid characteristic_uuid{ std::string_view(device->characteristic_uuid) };
        std::wstring target_name(device->device_name, device->device_name + strlen(device->device_name));

        SetBleState(device, EBleState::Scanning, u8"正在扫描 BLE 设备...");

        std::mutex found_lock;
        bool found = false;
        uint64_t address = 0;

        BluetoothLEAdvertisementWatcher watcher;
        watcher.ScanningMode(BluetoothLEScanningMode::Active);
        watcher.Received([&](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementReceivedEventArgs const& args) {
            if (device->stop_ble || found)
            {
                return;
            }

            bool service_match = false;
            for (auto const& uuid : args.Advertisement().ServiceUuids())
            {
                if (uuid == service_uuid)
                {
                    service_match = true;
                    break;
                }
            }

            std::wstring local_name = args.Advertisement().LocalName().c_str();
            bool name_match = !target_name.empty() && local_name == target_name;
            if (!service_match && !name_match)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(found_lock);
            found = true;
            address = args.BluetoothAddress();
        });

        watcher.Start();
        auto scan_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (!device->stop_ble && std::chrono::steady_clock::now() < scan_deadline)
        {
            {
                std::lock_guard<std::mutex> lock(found_lock);
                if (found)
                {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        watcher.Stop();

        if (device->stop_ble)
        {
            return;
        }
        if (!found)
        {
            SetBleState(device, EBleState::Error, u8"扫描超时：没有找到设备广播名或目标 Service UUID");
            return;
        }

        SetBleState(device, EBleState::Connecting, u8"已发现设备，正在连接 GATT...");
        BluetoothLEDevice ble_device = BluetoothLEDevice::FromBluetoothAddressAsync(address).get();
        if (ble_device == nullptr)
        {
            SetBleState(device, EBleState::Error, u8"BLE 设备连接失败");
            return;
        }

        auto services_result = ble_device.GetGattServicesForUuidAsync(service_uuid, BluetoothCacheMode::Uncached).get();
        if (services_result.Status() != GattCommunicationStatus::Success || services_result.Services().Size() == 0)
        {
            SetBleState(device, EBleState::Error, u8"未找到 BLE Service");
            return;
        }

        GattDeviceService service = services_result.Services().GetAt(0);
        auto characteristics_result = service.GetCharacteristicsForUuidAsync(characteristic_uuid, BluetoothCacheMode::Uncached).get();
        if (characteristics_result.Status() != GattCommunicationStatus::Success || characteristics_result.Characteristics().Size() == 0)
        {
            SetBleState(device, EBleState::Error, u8"未找到 BLE Characteristic");
            return;
        }

        GattCharacteristic characteristic = characteristics_result.Characteristics().GetAt(0);
        auto token = characteristic.ValueChanged([this, device](GattCharacteristic const&, GattValueChangedEventArgs const& args) {
            auto buffer = args.CharacteristicValue();
            DataReader reader = DataReader::FromBuffer(buffer);
            std::string packet;
            packet.resize(buffer.Length());
            if (!packet.empty())
            {
                reader.ReadBytes(array_view<uint8_t>(reinterpret_cast<uint8_t*>(&packet[0]), reinterpret_cast<uint8_t*>(&packet[0]) + packet.size()));
                OnBlePacketReceived(device, packet);
            }
        });

        auto notify_status = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
        if (notify_status != GattCommunicationStatus::Success)
        {
            characteristic.ValueChanged(token);
            SetBleState(device, EBleState::Error, u8"开启 BLE Notify 失败");
            return;
        }

        SetBleState(device, EBleState::Connected, u8"BLE 已连接，正在接收 Notify 数据");
        while (!device->stop_ble)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::None).get();
        characteristic.ValueChanged(token);
    }
    catch (const hresult_error& e)
    {
        std::string msg = u8"BLE 异常：";
        msg += winrt::to_string(e.message());
        SetBleState(device, EBleState::Error, msg);
    }
    catch (const std::exception& e)
    {
        std::string msg = u8"BLE 异常：";
        msg += e.what();
        SetBleState(device, EBleState::Error, msg);
    }
}

void UIBeamCalibration::DrawBleDevice(size_t index, SBleBeamDevice* device)
{
    if (device == nullptr)
    {
        return;
    }

    char buf[256] = { 0 };
    ST_BeamBleData data;
    {
        std::lock_guard<std::mutex> lock(device->data_lock);
        data = device->current_data;
    }

    EBleState state;
    std::string state_msg;
    std::string last_packet;
    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        state = device->state;
        state_msg = device->state_msg;
        last_packet = device->last_packet;
    }

    bool busy = state == EBleState::Scanning || state == EBleState::Connecting || state == EBleState::Disconnecting;
    bool connected = state == EBleState::Connected;

    stbsp_sprintf(buf, "%s##beam_ble_%u", device->display_name, device->id);
    if (!ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::PushID(static_cast<int>(device->id));
    ImGui::BeginDisabled(connected || busy || m_calibration_run_flag);
    ImGui::InputText(u8"显示名称", device->display_name, IM_ARRAYSIZE(device->display_name));
    ImGui::InputText(u8"设备广播名", device->device_name, IM_ARRAYSIZE(device->device_name));
    ImGui::InputText(u8"Service UUID", device->service_uuid, IM_ARRAYSIZE(device->service_uuid));
    ImGui::InputText(u8"Characteristic UUID", device->characteristic_uuid, IM_ARRAYSIZE(device->characteristic_uuid));
    ImGui::EndDisabled();

    ImGui::TextWrapped(u8"状态：%s", state_msg.c_str());
    if (!last_packet.empty())
    {
        ImGui::TextWrapped(u8"最近数据：%s", last_packet.c_str());
    }

    ImGui::BeginDisabled(m_calibration_run_flag);
    if (connected || busy)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.00f, 0.40f, 0.40f, 1.00f));
        if (ImGui::Button(u8"断开 BLE", ImVec2(130.0f, 36.0f)))
        {
            StopBleConnect(device);
        }
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::Button(u8"BLE 连接", ImVec2(130.0f, 36.0f)))
        {
            StartBleConnect(device);
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM / BEAM_NUM; ++i)
    {
        stbsp_sprintf(buf, u8"设备%zu-通道%d(mV)", index + 1, i + 1);
        float mv = data.channel_mv[i];
        ImGui::InputFloat(buf, &mv, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::PopID();
}

void UIBeamCalibration::DrawBleConfig()
{
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), u8"%s BLE配置", ICON_FA_PLUG);
    ImGui::Separator();

    ImGui::BeginDisabled(m_calibration_run_flag);
    if (ImGui::Button(u8"全部连接", ImVec2(120.0f, 36.0f)))
    {
        for (auto& device : m_ble_devices)
        {
            if (!IsBleBusyOrConnected(device))
            {
                StartBleConnect(&device);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"全部断开", ImVec2(120.0f, 36.0f)))
    {
        for (auto& device : m_ble_devices)
        {
            StopBleConnect(&device);
        }
    }
    ImGui::EndDisabled();

    for (size_t i = 0; i < m_ble_devices.size(); ++i)
    {
        DrawBleDevice(i, &m_ble_devices[i]);
    }
}
void UIBeamCalibration::Draw()
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

    // 获取可用宽度
    float avail_width = ImGui::GetContentRegionAvail().x;

    // 计算文本宽度
    const char* title = u8"弹性板弯扭结合标定";
    ImVec2 text_size = ImGui::CalcTextSize(title);

    // 计算居中位置并绘制
    float pos_x = (avail_width - text_size.x) * 0.5f;
    if (pos_x > 0) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pos_x);
    }
    ImGui::Text(title);
    // 使用Child窗口实现真正的分栏
    float config_height = 400;  // 配置区域高度
    ImGui::BeginChild("BleConfig", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, config_height), true);
    {
        DrawBleConfig();
    }
    ImGui::EndChild();
    // 中间间隔
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();

    ImGui::BeginChild("RobotConfig", ImVec2(0, config_height), true);
    {
        //ImGui::BeginDisabled(m_calibration_run_flag);

        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), u8"%s 机器人配置", ICON_FA_ROBOT);
        ImGui::Separator();
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
            if (ImGui::Button(u8"打开电源"))
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
            if (ImGui::Button(u8"关闭电源"))
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
            if (ImGui::Button(u8"关机"))
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
        float joints[6] = { 0 };
        float tcp[6] = { 0 };
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
        static float joints[6] = { 0 };
        static float tcp[6] = { 0 };

        ImGui::SliderFloat(u8"UR运动速度(m/s)", &m_ur_move_speed, 0.01f, 0.50f, "%.2f");
        ImGui::SliderFloat(u8"UR运动加速度(m/s^2)", &m_ur_move_accel, 0.05f, 2.00f, "%.2f");

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

    }
    ImGui::EndChild();

    // ========== 第二行：标定控制 ==========
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), u8"标定控制");

    // 在 ImGui 绘制循环中
    if (ImGui::Button(u8"选择轨迹文件", ImVec2(-1, 0))) {
        // 弹出系统文件对话框，过滤 CSV
        auto selection = pfd::open_file(
            u8"选择轨迹文件",               // 标题
            "",                              // 默认路径（空表示当前目录）
            { "CSV 轨迹文件", "*.csv" },     // 过滤器
            pfd::opt::none
        ).result();

        if (!selection.empty()) {
            m_loaded_filename = selection[0];  // 拿到完整路径

            // 直接复用你原来的解析逻辑
            bool success = ParseCSVFile(m_loaded_filename);
            if (success) {
                LOG_INFO("Load Success! %zu poses in total", m_calibration_aim_poses.size());
            }
            else {
                LOG_INFO("Load Fail: %s", m_loaded_filename.c_str());
            }
        }
    }

    // 3. (可选) 在按钮旁边显示当前加载的数据量，方便调试
    ImGui::SameLine();
    ImGui::Text(u8"当前数据量: %zu 行", m_calibration_aim_poses.size());

    ImGui::InputInt(u8"实验重复次数", &m_attempt_num);
    ImGui::InputFloat(u8"延迟记录时间(s)", &m_wait_time_sec, 0.1f, 1.0f, "%.2f");
    if (m_wait_time_sec < 0.0f)
    {
        m_wait_time_sec = 0.0f;
    }
    if (!m_calibration_run_flag)
    {
        //if (ImGui::Button(u8"开始标定", ImVec2(-1, 0)))
        //{
        //    m_calibration_run_flag = true;
        //    m_calibration_thread = std::thread(&UIBeamCalibration::CalibrationThreadFunc, this);
        //}


        if (ImGui::Button(u8"自动标定")) {
            if (m_calibration_thread.joinable())
            {
                m_calibration_thread.join();
            }
            m_calibration_run_flag = true;
            m_auto_adjust_flag = true;
            m_calibration_thread = std::thread(&UIBeamCalibration::CalibrationThreadFunc, this);
        }
        ImGui::SameLine();  // 让下一个控件紧接在同一行
        if (ImGui::Button(u8"手动标定")) {
            if (m_calibration_thread.joinable())
            {
                m_calibration_thread.join();
            }
            m_calibration_run_flag = true;
            m_auto_adjust_flag = false;
            m_calibration_thread = std::thread(&UIBeamCalibration::CalibrationThreadFunc, this);
        }



    }
    else
    {
        if (!m_auto_adjust_flag)
        {
            if (ImGui::Button(u8"调整完毕，进行数据记录", ImVec2(-1, 0)))
            {
                m_adjust_finish_flag = true;
            }
        }

        if (ImGui::Button(u8"停止标定", ImVec2(-1, 0)))
        {
            m_calibration_run_flag = false;
            m_adjust_finish_flag = false;
            if (m_calibration_thread.joinable())
            {
                m_calibration_thread.join();
            }
        }
    }

    if (ImGui::CollapsingHeader(u8"实验数据", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX;
        if (ImGui::BeginTable("ExperimentDataTable", 19, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1); // 冻结第一行
            ImGui::TableSetupColumn(u8"序号", ImGuiTableColumnFlags_WidthFixed);
            for (int i = 0; i < 6; ++i)
            {
                stbsp_sprintf(buf, u8"POSE%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; ++i)
            {
                stbsp_sprintf(buf, u8"ADC%d", i + 1);
                ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthStretch);
            }
            ImGui::TableHeadersRow();

            std::vector<ST_CalibrationData> experiment_data;
            {
                std::lock_guard<std::mutex> lock(m_calibration_data_lock);
                experiment_data = m_calibration_data;
            }
            // 使用ImGuiListClipper来优化大数据量的渲染
            ImGuiListClipper clipper;
            clipper.Begin((int)experiment_data.size());
            while (clipper.Step())
            {
                for (size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    const ST_CalibrationData& data = experiment_data[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", i + 1);
                    for (int j = 0; j < 6; ++j)
                    {
                        ImGui::TableSetColumnIndex(1 + j);
                        ImGui::Text("%.3f", data.tcp[j]);
                    }
                    for (int j = 0; j < CALIBRATION_STRAIN_CHANNEL_NUM; ++j)
                    {
                        ImGui::TableSetColumnIndex(1+6 + j);
                        ImGui::Text("%.3f", data.com_data[j]);
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();

}

void UIBeamCalibration::CalibrationThreadFunc()
{

    for (m_temp_attempt = 1; m_temp_attempt <= m_attempt_num && m_calibration_run_flag; m_temp_attempt++)
    {
        // 清空实验数据
        {
            std::lock_guard<std::mutex> lock(m_calibration_data_lock);
            m_calibration_data.clear();
        }

        LOG_INFO("Temp attempt is %d", m_temp_attempt);
        // 电机运动步长
//int step = 100;
        if (m_calibration_run_flag)
        {


            for (size_t CalibrationIndex = 0; CalibrationIndex < m_calibration_aim_poses.size() && m_calibration_run_flag; CalibrationIndex++)
            {
                Pose CurPose = m_calibration_aim_poses[CalibrationIndex];

                LOG_INFO("Temp aim pose is %f,%f,%f,%f,%f,%f, confirm and move?", CurPose[0], CurPose[1], CurPose[2], CurPose[3], CurPose[4], CurPose[5]);
                //if is waiting for adjustment, stop in this cycle
                while ((!m_adjust_finish_flag && !m_auto_adjust_flag) && m_calibration_run_flag)
                {

                }

                m_adjust_finish_flag = false;

                //MOVE THE UR ROBOT
                static float joints[6] = { 0 };
                static float tcp[6] = { 0 };
                for (int i = 0; i < 6; ++i)
                {
                    tcp[i] = static_cast<float>(CurPose[i]);
                }
                std::vector<double> movel_pos(tcp, tcp + 6);

                if (m_calibration_run_flag)
                {
                    ////temp constraint
                    m_ur.MoveL(movel_pos, m_ur_move_speed, m_ur_move_accel);
                }

                int delay_ms = CalibrationIndex == 0 ? 2000 : static_cast<int>(m_wait_time_sec * 1000.0f);
                for (int elapsed_ms = 0; elapsed_ms < delay_ms && m_calibration_run_flag; elapsed_ms += 50)
                {
                    int sleep_ms = delay_ms - elapsed_ms;
                    if (sleep_ms > 50)
                    {
                        sleep_ms = 50;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }
                
                {
                    std::lock_guard<std::mutex> lock(m_sensors_data_lock);
                    CopyBleSensorData(m_sensors);
                }

                // 获取当前实验数据
                ST_CalibrationData data;
                for (int i = 0; i < 6; ++i)
                {
                    data.tcp[i] = static_cast<float>(CurPose[i]);
                }


                {
                    std::lock_guard<std::mutex> lock(m_sensors_data_lock);
                    for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; ++i)
                    {

                        data.com_data[i] = m_sensors[i];
                        //data.com_data[i] = 0;
                    }
                }


                // 保存实验数据
                {
                    std::lock_guard<std::mutex> lock(m_calibration_data_lock);
                    m_calibration_data.push_back(data);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));


            }
            ////结束循环后停止
            //m_experiment_run_flag = false;

        }

        // 将实验数据存储到CSV
        {
            std::lock_guard<std::mutex> lock(m_calibration_data_lock);
            if (!m_calibration_data.empty())
            {
                // 按日期时间和夹爪宽度格式化文件名
                time_t now = time(nullptr);
                struct tm* tm_info = localtime(&now);
                char filename[64];
                //No need for time
                //stbsp_snprintf(filename, sizeof(filename), "experiment_data_%.2fx_%.2fy_%d%02d%02d_%02d%02d%02d.csv",
                //                m_x_constraints, m_y_constraints, tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                //               tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
                stbsp_snprintf(filename, sizeof(filename), "experiment_data_%d.csv",
                    m_temp_attempt);
                LOG_INFO("Saving experiment data to: %s", filename);
                // 打开文件
                std::ofstream ofs(filename);
                if (!ofs.is_open())
                {
                    LOG_ERROR("Failed to open file: %s", filename);
                    return;
                }
                // 写入CSV头
                for (int i = 0; i < 6; ++i)
                {
                    ofs << "POSE" << (i + 1) << ",";
                }
                for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; ++i)
                {
                    ofs << "ADC" << (i + 1) << ",";
                }
                ofs << "Temp_Attempt,";
                ofs << "\n";
                // 写入实验数据
                ofs.precision(6);
                for (const auto& data : m_calibration_data)
                {
                    for (int i = 0; i < 6; ++i)
                    {
                        ofs << data.tcp[i] << ",";
                    }
                    for (int i = 0; i < CALIBRATION_STRAIN_CHANNEL_NUM; ++i)
                    {
                        ofs << data.com_data[i] << ",";
                    }
                    ofs << m_temp_attempt << ",";

                    ofs << "\n";
                }
            }
        }
    }

    m_calibration_run_flag = false;
    m_adjust_finish_flag = false;
}

// 内部逻辑：解析文件的核心函数
bool UIBeamCalibration::ParseCSVFile(const std::string& filename) {
    std::vector<Pose> temp_poses;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "错误：无法打开文件 " << filename << std::endl;
        return false;
    }

    std::vector<std::vector<double>> data;
    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;

        // 跳过空行
        if (line.empty()) continue;

        // 处理UTF-8 BOM头（第一行）
        if (lineNum == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }

        std::stringstream ss(line);
        std::string cell;
        Pose pose = { 0, 0, 0, 0, 0, 0 };  // 初始化6个元素
        int col = 0;

        // 分割并填充6列
        while (std::getline(ss, cell, ',') && col < 6) {
            // 清理空白字符
            cell.erase(
                std::remove_if(cell.begin(), cell.end(),
                    [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }),
                cell.end()
            );

            if (cell.empty()) {
                col++;
                continue;
            }

            try {
                pose[col] = std::stod(cell);
            }
            catch (...) {
                // 解析失败，保持默认值0
            }
            col++;
        }

        // 只接受恰好6列的数据
        if (col == 6) {
            temp_poses.push_back(pose);
        }
    }
    file.close();

    if (!temp_poses.empty()) {
        m_calibration_aim_poses = std::move(temp_poses);
        return true;
    }
    return false;
}
