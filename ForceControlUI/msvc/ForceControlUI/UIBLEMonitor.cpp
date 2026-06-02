#include "UIBLEMonitor.h"

#include "IconsFontAwesome6.h"
#include "Logger.h"
#include "UIUtils.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include "stb/stb_sprintf.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

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

UIBLEMonitor::UIBLEMonitor(UIMainWindowBase* main_win, const char* title)
    : UIBaseWindow(main_win, title)
{
    AddDevice();
}

UIBLEMonitor::~UIBLEMonitor()
{
    StopAllDevices();
}

void UIBLEMonitor::AddDevice()
{
    auto device = std::make_unique<SBleDeviceMonitor>();
    device->id = m_next_device_id++;
    stbsp_sprintf(device->display_name, u8"BLE设备%u", device->id);
    stbsp_sprintf(device->device_name, "nRF52840_%02u", device->id);
    stbsp_sprintf(device->service_uuid, "19B1%02u00-E8F2-537E-4F6C-D104768A1214", device->id);
    strcpy(device->characteristic_uuid, STRAIN_CHARACTERISTIC_UUID);
    device->hist_data.resize(BLE_STRAIN_CHANNEL_NUM);
    device->serialize_thread = std::thread(&UIBLEMonitor::SerializeThreadFunc, this, device.get());
    m_devices.push_back(std::move(device));
}

void UIBLEMonitor::RemoveDevice(size_t index)
{
    if (index >= m_devices.size())
    {
        return;
    }

    SBleDeviceMonitor* device = m_devices[index].get();
    StopConnect(device);
    device->serialize_run_flag = false;
    if (device->serialize_thread.joinable())
    {
        device->serialize_thread.join();
    }
    m_devices.erase(m_devices.begin() + index);
}

bool UIBLEMonitor::IsBusyOrConnected(const SBleDeviceMonitor& device) const
{
    std::lock_guard<std::mutex> lock(device.state_lock);
    return device.state == EBleState::Scanning ||
        device.state == EBleState::Connecting ||
        device.state == EBleState::Connected ||
        device.state == EBleState::Disconnecting;
}

bool UIBLEMonitor::IsConnected(const SBleDeviceMonitor& device) const
{
    std::lock_guard<std::mutex> lock(device.state_lock);
    return device.state == EBleState::Connected;
}

void UIBLEMonitor::StopAllDevices()
{
    for (auto& device : m_devices)
    {
        StopConnect(device.get());
        device->serialize_run_flag = false;
    }

    for (auto& device : m_devices)
    {
        if (device->serialize_thread.joinable())
        {
            device->serialize_thread.join();
        }
    }
}

void UIBLEMonitor::StartSerializeAllConnected()
{
    for (auto& device : m_devices)
    {
        if (!IsConnected(*device))
        {
            continue;
        }

        std::lock_guard<std::mutex> lock(device->record_lock);
        device->record_data.clear();
        device->start_serialize = true;
    }
}

void UIBLEMonitor::StopSerializeAll()
{
    for (auto& device : m_devices)
    {
        device->start_serialize = false;
    }
}

void UIBLEMonitor::SetState(SBleDeviceMonitor* device, EBleState state, const std::string& message)
{
    if (device == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(device->state_lock);
    device->state = state;
    device->state_msg = message;
}

void UIBLEMonitor::StartConnect(SBleDeviceMonitor* device)
{
    if (device == nullptr)
    {
        return;
    }

    StopConnect(device);
    device->stop_ble = false;
    device->start_time = std::chrono::steady_clock::now();
    device->last_packet_time = device->start_time;
    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        device->last_packet.clear();
        device->packet_count = 0;
    }
    {
        std::lock_guard<std::mutex> lock(device->hist_lock);
        device->hist_data.clear();
        device->hist_data.resize(BLE_STRAIN_CHANNEL_NUM);
    }
    device->ble_thread = std::thread(&UIBLEMonitor::BleThreadFunc, this, device);
}

void UIBLEMonitor::StopConnect(SBleDeviceMonitor* device)
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
    device->start_serialize = false;
    SetState(device, EBleState::Disconnected, u8"未连接");
}

bool UIBLEMonitor::ParsePacket(const std::string& packet, ST_BleStrainData& data)
{
    std::stringstream ss(packet);
    std::string token;
    int index = 0;

    while (std::getline(ss, token, ',') && index < BLE_STRAIN_CHANNEL_NUM)
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

    return index == BLE_STRAIN_CHANNEL_NUM;
}

void UIBLEMonitor::OnPacketReceived(SBleDeviceMonitor* device, const std::string& packet)
{
    if (device == nullptr)
    {
        return;
    }

    ST_BleStrainData data;
    if (!ParsePacket(packet, data))
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - m_plot_start_time).count();

    {
        std::lock_guard<std::mutex> lock(device->data_lock);
        device->current_data = data;
        device->last_packet_time = now;
    }

    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        device->last_packet = packet;
        ++device->packet_count;
    }

    {
        std::lock_guard<std::mutex> lock(device->hist_lock);
        for (int i = 0; i < BLE_STRAIN_CHANNEL_NUM; ++i)
        {
            device->hist_data[i].AddPoint(t, data.channel_mv[i]);
        }
    }

    if (device->start_serialize)
    {
        std::lock_guard<std::mutex> lock(device->record_lock);
        device->record_data.push_back(data);
    }
}

void UIBLEMonitor::BleThreadFunc(SBleDeviceMonitor* device)
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

        SetState(device, EBleState::Scanning, u8"正在扫描 BLE 设备...");

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

            {
                std::lock_guard<std::mutex> lock(found_lock);
                found = true;
                address = args.BluetoothAddress();
            }
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
            SetState(device, EBleState::Error, u8"扫描超时：没有找到设备广播名或目标 Service UUID");
            return;
        }

        SetState(device, EBleState::Connecting, u8"已发现设备，正在连接 GATT...");
        BluetoothLEDevice ble_device = BluetoothLEDevice::FromBluetoothAddressAsync(address).get();
        if (ble_device == nullptr)
        {
            SetState(device, EBleState::Error, u8"BLE 设备连接失败");
            return;
        }

        auto services_result = ble_device.GetGattServicesForUuidAsync(service_uuid, BluetoothCacheMode::Uncached).get();
        if (services_result.Status() != GattCommunicationStatus::Success || services_result.Services().Size() == 0)
        {
            SetState(device, EBleState::Error, u8"未找到 BLE Service");
            return;
        }

        GattDeviceService service = services_result.Services().GetAt(0);
        auto characteristics_result = service.GetCharacteristicsForUuidAsync(characteristic_uuid, BluetoothCacheMode::Uncached).get();
        if (characteristics_result.Status() != GattCommunicationStatus::Success || characteristics_result.Characteristics().Size() == 0)
        {
            SetState(device, EBleState::Error, u8"未找到 BLE Characteristic");
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
                OnPacketReceived(device, packet);
            }
        });

        auto notify_status = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
        if (notify_status != GattCommunicationStatus::Success)
        {
            characteristic.ValueChanged(token);
            SetState(device, EBleState::Error, u8"开启 BLE Notify 失败");
            return;
        }

        SetState(device, EBleState::Connected, u8"BLE 已连接，正在接收 Notify 数据");
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
        SetState(device, EBleState::Error, msg);
    }
    catch (const std::exception& e)
    {
        std::string msg = u8"BLE 异常：";
        msg += e.what();
        SetState(device, EBleState::Error, msg);
    }
}

void UIBLEMonitor::SerializeThreadFunc(SBleDeviceMonitor* device)
{
    if (device == nullptr)
    {
        return;
    }

    FILE* fp = nullptr;
    std::vector<ST_BleStrainData> pending;

    while (device->serialize_run_flag)
    {
        if (device->start_serialize)
        {
            if (fp == nullptr)
            {
                time_t now = time(nullptr);
                tm ltm = {};
                localtime_s(&ltm, &now);
                char filename[128] = { 0 };
                stbsp_sprintf(filename, "ble_device_%u_%04d%02d%02d_%02d%02d%02d.txt",
                    device->id,
                    ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
                    ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
                fp = fopen(filename, "w");
                if (fp)
                {
                    fprintf(fp, "ch1_mV,ch2_mV,ch3_mV,ch4_mV\n");
                    LOG_INFO(u8"数据采集开始，文件名：%s", filename);
                }
            }

            {
                std::lock_guard<std::mutex> lock(device->record_lock);
                pending.swap(device->record_data);
            }

            if (fp)
            {
                for (const auto& data : pending)
                {
                    fprintf(fp, "%.3f,%.3f,%.3f,%.3f\n",
                        data.channel_mv[0], data.channel_mv[1], data.channel_mv[2], data.channel_mv[3]);
                }
                fflush(fp);
            }
            pending.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else
        {
            if (fp)
            {
                fclose(fp);
                fp = nullptr;
                LOG_INFO(u8"数据采集完成");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if (fp)
    {
        fclose(fp);
    }
}

void UIBLEMonitor::DrawDevice(size_t index, SBleDeviceMonitor* device)
{
    if (device == nullptr)
    {
        return;
    }

    char buf[256] = { 0 };
    ST_BleStrainData data;
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

    stbsp_sprintf(buf, "%s##ble_device_%u", device->display_name, device->id);
    if (!ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::PushID(static_cast<int>(device->id));

    ImGui::BeginDisabled(connected || busy);
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

    if (connected || busy)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.00f, 0.40f, 0.40f, 1.00f));
        if (ImGui::Button(u8"断开 BLE", ImVec2(160.0f, 40.0f)))
        {
            StopConnect(device);
        }
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::Button(u8"BLE 连接", ImVec2(160.0f, 40.0f)))
        {
            StartConnect(device);
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(connected || busy || m_devices.size() <= 1);
    if (ImGui::Button(u8"删除设备", ImVec2(120.0f, 40.0f)))
    {
        ImGui::EndDisabled();
        ImGui::PopID();
        RemoveDevice(index);
        return;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    for (int i = 0; i < BLE_STRAIN_CHANNEL_NUM; ++i)
    {
        stbsp_sprintf(buf, u8"通道%d电压(mV)", i + 1);
        float mv = data.channel_mv[i];
        ImGui::InputFloat(buf, &mv, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }

    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - m_plot_start_time).count();
    std::vector<ScrollingBuffer> hist(BLE_STRAIN_CHANNEL_NUM);
    {
        std::lock_guard<std::mutex> lock(device->hist_lock);
        hist = device->hist_data;
    }

    stbsp_sprintf(buf, "##ble_plot_%u", device->id);
    if (ImPlot::BeginPlot(buf, ImVec2(-1, 240)))
    {
        ImPlot::SetupAxes("time(s)", "mV", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, t - m_history_seconds, t, ImGuiCond_Always);
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2);
        for (int i = 0; i < BLE_STRAIN_CHANNEL_NUM; ++i)
        {
            if (hist[i].Data.size())
            {
                stbsp_sprintf(buf, "CH%d", i + 1);
                ImPlot::PlotLine(buf, &hist[i].Data[0].x, &hist[i].Data[0].y,
                    hist[i].Data.size(), ImPlotLineFlags_None, hist[i].Offset, 2 * sizeof(float));
            }
        }
        ImPlot::PopStyleVar(1);
        ImPlot::EndPlot();
    }

    ImGui::BeginDisabled(!connected);
    if (!device->start_serialize)
    {
        stbsp_sprintf(buf, u8"%s 开始采集", ICON_FA_CIRCLE_PLAY);
        if (ImGui::Button(buf))
        {
            std::lock_guard<std::mutex> lock(device->record_lock);
            device->record_data.clear();
            device->start_serialize = true;
        }
    }
    else
    {
        stbsp_sprintf(buf, u8"%s 停止采集", ICON_FA_CIRCLE_STOP);
        if (ImGui::Button(buf))
        {
            device->start_serialize = false;
        }
    }
    ImGui::EndDisabled();

    ImGui::PopID();
}

void UIBLEMonitor::Draw()
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

    if (ImGui::CollapsingHeader(u8"多设备控制", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button(u8"添加 BLE 设备", ImVec2(150.0f, 36.0f)))
        {
            AddDevice();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"全部连接", ImVec2(120.0f, 36.0f)))
        {
            for (auto& device : m_devices)
            {
                if (!IsBusyOrConnected(*device))
                {
                    StartConnect(device.get());
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"全部断开", ImVec2(120.0f, 36.0f)))
        {
            for (auto& device : m_devices)
            {
                StopConnect(device.get());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"全部开始采集", ImVec2(140.0f, 36.0f)))
        {
            StartSerializeAllConnected();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"全部停止采集", ImVec2(140.0f, 36.0f)))
        {
            StopSerializeAll();
        }
        ImGui::SliderFloat(u8"Plot时长", &m_history_seconds, 1.0f, 120.0f, "%.1f s");
    }

    for (size_t i = 0; i < m_devices.size();)
    {
        size_t before_size = m_devices.size();
        DrawDevice(i, m_devices[i].get());
        if (m_devices.size() == before_size)
        {
            ++i;
        }
    }

    ImGui::End();
}
