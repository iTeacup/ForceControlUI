#include "UIOptSampling.h"

#include "UIApplication.h"
#include "IconsFontAwesome6.h"
#include "Logger.h"
#include "UIUtils.h"
#include "imgui/imgui.h"
#include "stb/stb_sprintf.h"

#include <CPSAPI/CPSAPI.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
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

namespace
{
bool EnsureSaveDirectory(const char* dir)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        UI_WARN(u8"创建目录%s失败：%s", dir, ec.message().c_str());
        return false;
    }
    return true;
}

void GetTimeStr(char* dt, size_t len)
{
    time_t t = time(nullptr);
    tm ltm = {};
    localtime_s(&ltm, &t);
    strftime(dt, len, "%Y%m%d%H%M%S", &ltm);
}
}

UIOptSampling::UIOptSampling(UIMainWindowBase* main_win, const char* title)
    : UIBaseWindow(main_win, title)
{
    InitBleDevices();
    m_selected_ids.resize(m_disp_marker_count, -1);
}

UIOptSampling::~UIOptSampling()
{
    m_recording = false;
    if (m_sampling_thread.joinable())
    {
        m_sampling_thread.join();
    }
    StopAllBleDevices();
}

double UIOptSampling::NowSeconds() const
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_time_zero).count();
}

void UIOptSampling::DrawTitle()
{
    const char* title = u8"OptiTrack + BLE 同步采样";
    float avail_width = ImGui::GetContentRegionAvail().x;
    ImVec2 text_size = ImGui::CalcTextSize(title);
    float pos_x = (avail_width - text_size.x) * 0.5f;
    if (pos_x > 0.0f)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pos_x);
    }
    ImGui::Text(title);
}

void UIOptSampling::Draw()
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

    DrawTitle();

    float config_height = 390.0f;
    ImGui::BeginChild("OptSamplingBleConfig", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5.0f, config_height), true);
    DrawBleConfig();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10.0f, 0.0f));
    ImGui::SameLine();

    ImGui::BeginChild("OptSamplingMarkerConfig", ImVec2(0.0f, config_height), true);
    DrawOptConfig();
    ImGui::EndChild();

    ImGui::Separator();
    DrawSamplingControl();
    DrawRecordTable();

    ImGui::End();
}

void UIOptSampling::DrawBleConfig()
{
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), u8"%s BLE配置", ICON_FA_PLUG);
    ImGui::Separator();

    bool recording = m_recording.load();
    ImGui::BeginDisabled(recording);
    if (ImGui::Button(u8"全部连接", ImVec2(120.0f, 34.0f)))
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
    if (ImGui::Button(u8"全部断开", ImVec2(120.0f, 34.0f)))
    {
        StopAllBleDevices();
    }
    ImGui::EndDisabled();

    for (size_t i = 0; i < m_ble_devices.size(); ++i)
    {
        DrawBleDevice(i, &m_ble_devices[i]);
    }
}

void UIOptSampling::DrawBleDevice(size_t index, SBleOptDevice* device)
{
    if (device == nullptr)
    {
        return;
    }

    char buf[256] = { 0 };
    ST_OptSamplingBleData data;
    {
        std::lock_guard<std::mutex> lock(device->data_lock);
        data = device->current_data;
    }

    EBleState state;
    std::string state_msg;
    std::string last_packet_text;
    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        state = device->state;
        state_msg = device->state_msg;
        last_packet_text = device->last_packet_text;
    }

    bool busy = state == EBleState::Scanning || state == EBleState::Connecting || state == EBleState::Disconnecting;
    bool connected = state == EBleState::Connected;
    bool recording = m_recording.load();

    stbsp_sprintf(buf, "%s##opt_ble_%u", device->display_name, device->id);
    if (!ImGui::CollapsingHeader(buf, ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::PushID(static_cast<int>(device->id));
    ImGui::BeginDisabled(connected || busy || recording);
    ImGui::InputText(u8"显示名称", device->display_name, IM_ARRAYSIZE(device->display_name));
    ImGui::InputText(u8"设备广播名", device->device_name, IM_ARRAYSIZE(device->device_name));
    ImGui::InputText(u8"Service UUID", device->service_uuid, IM_ARRAYSIZE(device->service_uuid));
    ImGui::InputText(u8"Characteristic UUID", device->characteristic_uuid, IM_ARRAYSIZE(device->characteristic_uuid));
    ImGui::EndDisabled();

    ImGui::TextWrapped(u8"状态：%s", state_msg.c_str());
    if (!last_packet_text.empty())
    {
        ImGui::TextWrapped(u8"最近数据：%s", last_packet_text.c_str());
    }

    ImGui::BeginDisabled(recording);
    if (connected || busy)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.00f, 0.40f, 0.40f, 1.00f));
        if (ImGui::Button(u8"断开 BLE", ImVec2(130.0f, 34.0f)))
        {
            StopBleConnect(device);
        }
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::Button(u8"BLE 连接", ImVec2(130.0f, 34.0f)))
        {
            StartBleConnect(device);
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::InputScalar(u8"Timestamp(ms)", ImGuiDataType_U32, &data.timestamp, nullptr, nullptr, nullptr, ImGuiInputTextFlags_ReadOnly);
    for (int i = 0; i < OPT_SAMPLING_BLE_CHANNEL_NUM; ++i)
    {
        stbsp_sprintf(buf, u8"设备%zu-通道%d(mV)", index + 1, i + 1);
        float mv = data.channel_mv[i];
        ImGui::InputFloat(buf, &mv, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::PopID();
}

void UIOptSampling::DrawOptConfig()
{
    char buf[256] = { 0 };
    bool opt_online = g_app.GetCPSApi()->IsDeviceOnline(OPT_SERVER_DEV_ID);
    bool recording = m_recording.load();

    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), u8"%s OptiTrack配置", ICON_FA_CUBE);
    ImGui::Separator();
    ImGui::Text(u8"服务状态：%s", opt_online ? u8"在线" : u8"离线");

    ImGui::BeginDisabled(recording);
    if (ImGui::Button(u8"初始化服务", ImVec2(120.0f, 34.0f)))
    {
        InitOptService();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"停止服务", ImVec2(120.0f, 34.0f)))
    {
        StopOptService();
    }
    ImGui::EndDisabled();

    int marker_count = 0;
    {
        std::lock_guard<std::mutex> lock(m_selection_lock);
        marker_count = m_disp_marker_count;
    }

    ImGui::BeginDisabled(recording);
    if (ImGui::SliderInt(u8"记录Marker数量", &marker_count, 1, MAX_MARKER_NUM))
    {
        std::lock_guard<std::mutex> lock(m_selection_lock);
        m_disp_marker_count = marker_count;
        m_selected_ids.resize(m_disp_marker_count, -1);
    }
    ImGui::EndDisabled();

    for (int i = 0; i < marker_count; ++i)
    {
        ImGui::PushID(i);
        ImGui::Text("M%d", i + 1);
        ImGui::SameLine();

        char preview[64] = { 0 };
        int selected_id = -1;
        {
            std::lock_guard<std::mutex> lock(m_selection_lock);
            selected_id = m_selected_ids[i];
        }
        if (selected_id >= 0)
        {
            stbsp_snprintf(preview, sizeof(preview), "%d", selected_id);
        }

        ImGui::BeginDisabled(recording);
        ImGui::SetNextItemWidth(95.0f);
        if (ImGui::BeginCombo("##marker_id", preview, 0))
        {
            std::vector<int> marker_ids;
            std::lock_guard<std::mutex> lock(m_marker_list_lock);
            for (int n = 0; n < m_marker_list.marker_num; ++n)
            {
                marker_ids.push_back(m_marker_list.markers[n].ID);
            }
            std::sort(marker_ids.begin(), marker_ids.end());

            for (int id : marker_ids)
            {
                bool selected = selected_id == id;
                if (ImGui::Selectable(std::to_string(id).c_str(), selected))
                {
                    std::lock_guard<std::mutex> selection_lock(m_selection_lock);
                    m_selected_ids[i] = id;
                    selected_id = id;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        float xyz[3] = { 0.0f, 0.0f, 0.0f };
        int index = GetIndexByID(selected_id);
        if (index >= 0)
        {
            std::lock_guard<std::mutex> lock(m_marker_list_lock);
            memcpy(xyz, m_marker_list.markers[index].XYZ, sizeof(xyz));
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputFloat3("##XYZ", xyz, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::PopID();
    }
}

void UIOptSampling::DrawSamplingControl()
{
    bool opt_online = g_app.GetCPSApi()->IsDeviceOnline(OPT_SERVER_DEV_ID);
    bool ble_connected = IsAnyBleConnected();
    bool recording = m_recording.load();
    size_t record_count = 0;
    {
        std::lock_guard<std::mutex> lock(m_record_lock);
        record_count = m_records.size();
    }

    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), u8"同步采集");
    ImGui::SameLine();
    ImGui::Text(u8"记录行数：%llu", static_cast<unsigned long long>(record_count));

    ImGui::BeginDisabled(recording);
    ImGui::SliderFloat(u8"采样间隔(s)", &m_sample_interval_s, 0.1f, 1.0f, "%.2f");
    m_sample_interval_s = std::clamp(m_sample_interval_s, 0.1f, 1.0f);
    ImGui::SameLine();
    ImGui::Text(u8"%.1f Hz", 1.0f / m_sample_interval_s);
    ImGui::EndDisabled();

    if (!recording)
    {
        ImGui::BeginDisabled(!opt_online || !ble_connected);
        if (ImGui::Button(u8"开始同步采集", ImVec2(160.0f, 38.0f)))
        {
            StartRecording();
        }
        ImGui::EndDisabled();
    }
    else
    {
        if (ImGui::Button(u8"停止并保存CSV", ImVec2(160.0f, 38.0f)))
        {
            StopRecording();
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(recording || record_count == 0);
    if (ImGui::Button(u8"清空缓存", ImVec2(120.0f, 38.0f)))
    {
        ResetRecords();
    }
    ImGui::EndDisabled();
}

void UIOptSampling::DrawRecordTable()
{
    std::vector<SRecordRow> records;
    {
        std::lock_guard<std::mutex> lock(m_record_lock);
        records = m_records;
    }

    if (!ImGui::CollapsingHeader(u8"采集数据", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX;
    if (!ImGui::BeginTable("OptSamplingDataTable", 2 + OPT_SAMPLING_ADC_CHANNEL_NUM + m_disp_marker_count * 4, flags, ImVec2(-1, 220.0f)))
    {
        return;
    }

    char buf[64] = { 0 };
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(u8"序号", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn(u8"采样时间(s)", ImGuiTableColumnFlags_WidthFixed);
    for (int i = 0; i < OPT_SAMPLING_ADC_CHANNEL_NUM; ++i)
    {
        stbsp_sprintf(buf, "ADC%d(mV)", i + 1);
        ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthFixed);
    }
    for (int slot = 0; slot < m_disp_marker_count && slot < MAX_MARKER_NUM; ++slot)
    {
        stbsp_sprintf(buf, "M%d_ID", slot + 1);
        ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthFixed);
        stbsp_sprintf(buf, "M%d_X(mm)", slot + 1);
        ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthFixed);
        stbsp_sprintf(buf, "M%d_Y(mm)", slot + 1);
        ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthFixed);
        stbsp_sprintf(buf, "M%d_Z(mm)", slot + 1);
        ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthFixed);
    }
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(records.size()));
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const auto& row = records[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i + 1);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", row.sample_time_s);

            int column = 2;
            for (int device = 0; device < OPT_SAMPLING_BLE_DEVICE_NUM; ++device)
            {
                for (int channel = 0; channel < OPT_SAMPLING_BLE_CHANNEL_NUM; ++channel)
                {
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%.3f", row.ble[device].channel_mv[channel]);
                }
            }

            for (int slot = 0; slot < m_disp_marker_count && slot < MAX_MARKER_NUM; ++slot)
            {
                ImGui::TableSetColumnIndex(column++);
                if (row.marker_valid[slot])
                {
                    ImGui::Text("%d", row.markers[slot].ID);
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%.3f", row.markers[slot].XYZ[0]);
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%.3f", row.markers[slot].XYZ[1]);
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::Text("%.3f", row.markers[slot].XYZ[2]);
                }
                else
                {
                    ImGui::TextUnformatted("-");
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::TextUnformatted("-");
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::TextUnformatted("-");
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::TextUnformatted("-");
                }
            }
        }
    }
    if (m_recording.load() && records.size() != m_record_table_auto_scroll_count)
    {
        m_record_table_auto_scroll_count = records.size();
        ImGui::SetScrollY(ImGui::GetScrollMaxY());
    }
    ImGui::EndTable();
}

void UIOptSampling::InitBleDevices()
{
    for (size_t i = 0; i < m_ble_devices.size(); ++i)
    {
        SBleOptDevice& device = m_ble_devices[i];
        device.id = static_cast<uint32_t>(i + 1);
        stbsp_sprintf(device.display_name, u8"BLE设备%u", device.id);
        stbsp_sprintf(device.device_name, "nRF52840_%02u", device.id);
        stbsp_sprintf(device.service_uuid, "19B1%02u00-E8F2-537E-4F6C-D104768A1214", device.id);
        strcpy(device.characteristic_uuid, "19B10002-E8F2-537E-4F6C-D104768A1214");
    }
}

bool UIOptSampling::IsBleBusyOrConnected(const SBleOptDevice& device) const
{
    std::lock_guard<std::mutex> lock(device.state_lock);
    return device.state == EBleState::Scanning ||
        device.state == EBleState::Connecting ||
        device.state == EBleState::Connected ||
        device.state == EBleState::Disconnecting;
}

bool UIOptSampling::IsBleConnected(const SBleOptDevice& device) const
{
    std::lock_guard<std::mutex> lock(device.state_lock);
    return device.state == EBleState::Connected;
}

bool UIOptSampling::AreAllBleConnected() const
{
    return std::all_of(m_ble_devices.begin(), m_ble_devices.end(), [this](const SBleOptDevice& device) {
        return IsBleConnected(device);
    });
}

bool UIOptSampling::IsAnyBleConnected() const
{
    return std::any_of(m_ble_devices.begin(), m_ble_devices.end(), [this](const SBleOptDevice& device) {
        return IsBleConnected(device);
    });
}

void UIOptSampling::SetBleState(SBleOptDevice* device, EBleState state, const std::string& message)
{
    if (device == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(device->state_lock);
    device->state = state;
    device->state_msg = message;
}

void UIOptSampling::StartBleConnect(SBleOptDevice* device)
{
    if (device == nullptr)
    {
        return;
    }

    StopBleConnect(device);
    device->stop_ble = false;
    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        device->last_packet_text.clear();
        device->packet_count = 0;
    }
    {
        std::lock_guard<std::mutex> lock(device->data_lock);
        device->current_data = {};
    }
    device->ble_thread = std::thread(&UIOptSampling::BleThreadFunc, this, device);
}

void UIOptSampling::StopBleConnect(SBleOptDevice* device)
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

void UIOptSampling::StopAllBleDevices()
{
    for (auto& device : m_ble_devices)
    {
        StopBleConnect(&device);
    }
}

bool UIOptSampling::ParseBlePacket(const std::string& packet, ST_OptSamplingBleData& data)
{
    constexpr size_t expected_size = sizeof(uint32_t) + sizeof(float) * OPT_SAMPLING_BLE_CHANNEL_NUM;
    if (packet.size() != expected_size)
    {
        return false;
    }

    const char* src = packet.data();
    std::memcpy(&data.timestamp, src, sizeof(data.timestamp));
    src += sizeof(data.timestamp);
    std::memcpy(data.channel_mv, src, sizeof(data.channel_mv));
    return true;
}

void UIOptSampling::OnBlePacketReceived(SBleOptDevice* device, const std::string& packet)
{
    if (device == nullptr)
    {
        return;
    }

    ST_OptSamplingBleData data;
    if (!ParseBlePacket(packet, data))
    {
        return;
    }

    data.app_time_s = NowSeconds();
    data.valid = true;
    {
        std::lock_guard<std::mutex> lock(device->state_lock);
        data.packet_index = ++device->packet_count;
        char packet_text[128] = { 0 };
        stbsp_sprintf(packet_text, "%.3f, %.3f, %.3f, %.3f mV",
            data.channel_mv[0], data.channel_mv[1], data.channel_mv[2], data.channel_mv[3]);
        device->last_packet_text = packet_text;
    }
    {
        std::lock_guard<std::mutex> lock(device->data_lock);
        device->current_data = data;
    }
}

void UIOptSampling::CopyBleData(ST_OptSamplingBleData ble[OPT_SAMPLING_BLE_DEVICE_NUM])
{
    for (size_t i = 0; i < m_ble_devices.size(); ++i)
    {
        if (!IsBleConnected(m_ble_devices[i]))
        {
            ble[i] = {};
            continue;
        }
        std::lock_guard<std::mutex> lock(m_ble_devices[i].data_lock);
        ble[i] = m_ble_devices[i].current_data;
    }
}

void UIOptSampling::BleThreadFunc(SBleOptDevice* device)
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

void UIOptSampling::OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len)
{
    switch (msg_type)
    {
    case MSG_OPTITRACK_MARKER_INFO:
        OnMarkerListMsg(data, msg_len);
        break;
    case MSG_CMD_INIT_RSP:
        OnInitRsp(data, msg_len);
        break;
    case MSG_CMD_STOP_RSP:
        OnStopRsp(data, msg_len);
        break;
    default:
        break;
    }
}

void UIOptSampling::OnInitRsp(const char* data, uint32_t msg_len)
{
    if (msg_len < sizeof(ST_CMDInitRsp))
    {
        return;
    }

    ST_CMDInitRsp* rsp = (ST_CMDInitRsp*)data;
    if (rsp->rsp.error_code == 0)
    {
        UI_INFO(u8"[%d]-OptiTrack 服务初始化成功！", rsp->req_no);
    }
    else
    {
        UI_ERROR(u8"[%d]-OptiTrack 服务初始化失败！原因：[%d]-%s", rsp->req_no, rsp->rsp.error_code, rsp->rsp.error_msg);
    }
}

void UIOptSampling::OnStopRsp(const char* data, uint32_t msg_len)
{
    if (msg_len < sizeof(ST_CMDStopRsp))
    {
        return;
    }

    ST_CMDStopRsp* rsp = (ST_CMDStopRsp*)data;
    if (rsp->rsp.error_code == 0)
    {
        UI_INFO(u8"[%d]-OptiTrack 服务停止成功！", rsp->req_no);
    }
    else
    {
        UI_ERROR(u8"[%d]-OptiTrack 服务停止失败！原因：[%d]-%s", rsp->req_no, rsp->rsp.error_code, rsp->rsp.error_msg);
    }
}

void UIOptSampling::OnMarkerListMsg(const char* data, uint32_t msg_len)
{
    if (msg_len < sizeof(ST_OptMarker_List))
    {
        return;
    }

    ST_OptMarker_List marker_list = {};
    memcpy(&marker_list, data, sizeof(ST_OptMarker_List));
    {
        std::lock_guard<std::mutex> lock(m_marker_list_lock);
        m_marker_list = marker_list;
    }
}

int UIOptSampling::GetIndexByID(int id)
{
    std::lock_guard<std::mutex> lock(m_marker_list_lock);
    for (int i = 0; i < m_marker_list.marker_num; ++i)
    {
        if (m_marker_list.markers[i].ID == id)
        {
            return i;
        }
    }
    return -1;
}

void UIOptSampling::InitOptService()
{
    if (!g_app.GetCPSApi()->IsDeviceOnline(OPT_SERVER_DEV_ID))
    {
        UI_ERROR(u8"设备[%d]不在线！", OPT_SERVER_DEV_ID);
        return;
    }

    ST_CMDInit req = { m_req_id++ };
    g_app.GetCPSApi()->SendAPPMsg(OPT_SERVER_DEV_ID, MSG_CMD_INIT, (const char*)&req, sizeof(ST_CMDInit));
}

void UIOptSampling::StopOptService()
{
    if (!g_app.GetCPSApi()->IsDeviceOnline(OPT_SERVER_DEV_ID))
    {
        UI_ERROR(u8"设备[%d]不在线！", OPT_SERVER_DEV_ID);
        return;
    }

    ST_CMDStop req = { m_req_id++ };
    g_app.GetCPSApi()->SendAPPMsg(OPT_SERVER_DEV_ID, MSG_CMD_STOP, (const char*)&req, sizeof(ST_CMDStop));
}

void UIOptSampling::StartRecording()
{
    if (!g_app.GetCPSApi()->IsDeviceOnline(OPT_SERVER_DEV_ID))
    {
        UI_WARN(u8"OptiTrack服务不在线，不能开始采集。");
        return;
    }
    if (!IsAnyBleConnected())
    {
        UI_WARN(u8"请至少连接一个BLE设备。");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_selection_lock);
        if (!std::any_of(m_selected_ids.begin(), m_selected_ids.end(), [](int id) { return id >= 0; }))
        {
            UI_WARN(u8"请至少选择一个Marker。");
            return;
        }
    }

    if (m_sampling_thread.joinable())
    {
        m_sampling_thread.join();
    }

    ResetRecords();
    m_time_zero = std::chrono::steady_clock::now();
    float interval_s = std::clamp(m_sample_interval_s, 0.1f, 1.0f);
    m_sample_interval_s = interval_s;
    m_recording = true;
    m_sampling_thread = std::thread(&UIOptSampling::SamplingThreadFunc, this, interval_s);
    UI_INFO(u8"Opt采样开始，采样间隔%.2fs。", interval_s);
}

void UIOptSampling::StopRecording()
{
    m_recording = false;
    if (m_sampling_thread.joinable())
    {
        m_sampling_thread.join();
    }
    SaveRecords();
}

void UIOptSampling::SamplingThreadFunc(float interval_s)
{
    interval_s = std::clamp(interval_s, 0.1f, 1.0f);
    auto interval = std::chrono::duration<double>(interval_s);
    auto next_tick = m_time_zero + interval;
    uint64_t sample_index = 1;

    while (m_recording.load())
    {
        std::this_thread::sleep_until(next_tick);
        if (!m_recording.load())
        {
            break;
        }

        double sample_time_s = static_cast<double>(sample_index) * interval_s;
        CaptureSample(sample_time_s);
        ++sample_index;
        next_tick += interval;
    }
}

void UIOptSampling::CaptureSample(double sample_time_s)
{
    SRecordRow row;
    row.sample_time_s = sample_time_s;
    CopyBleData(row.ble);

    ST_OptMarker_List marker_list = {};
    {
        std::lock_guard<std::mutex> lock(m_marker_list_lock);
        marker_list = m_marker_list;
    }

    std::vector<int> selected_ids;
    int selected_count = 0;
    {
        std::lock_guard<std::mutex> lock(m_selection_lock);
        selected_ids = m_selected_ids;
        selected_count = m_disp_marker_count;
    }

    for (int slot = 0; slot < selected_count && slot < MAX_MARKER_NUM; ++slot)
    {
        int id = selected_ids[slot];
        if (id < 0)
        {
            continue;
        }

        for (int i = 0; i < marker_list.marker_num; ++i)
        {
            if (marker_list.markers[i].ID == id)
            {
                row.markers[slot] = marker_list.markers[i];
                row.marker_valid[slot] = true;
                break;
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_record_lock);
    m_records.push_back(row);
}

void UIOptSampling::ResetRecords()
{
    std::lock_guard<std::mutex> lock(m_record_lock);
    m_records.clear();
    m_record_table_auto_scroll_count = 0;
}

void UIOptSampling::SaveRecords()
{
    std::vector<SRecordRow> records;
    {
        std::lock_guard<std::mutex> lock(m_record_lock);
        records = m_records;
    }

    if (records.empty())
    {
        UI_WARN(u8"没有同步采集数据，保存失败！");
        return;
    }
    if (!EnsureSaveDirectory("data/opt_sampling"))
    {
        return;
    }

    char dt[64] = { 0 };
    GetTimeStr(dt, sizeof(dt));
    char filename[256] = { 0 };
    stbsp_snprintf(filename, sizeof(filename), "data/opt_sampling/opt_sampling_%s.csv", dt);

    std::ofstream os(filename);
    if (!os.is_open())
    {
        UI_WARN(u8"打开文件%s失败！", filename);
        return;
    }

    std::vector<int> selected_ids;
    int selected_count = 0;
    {
        std::lock_guard<std::mutex> lock(m_selection_lock);
        selected_ids = m_selected_ids;
        selected_count = m_disp_marker_count;
    }

    os << std::fixed << std::setprecision(3);
    os << "sample_t(s)";
    for (int i = 0; i < OPT_SAMPLING_ADC_CHANNEL_NUM; ++i)
    {
        os << ",ADC" << i + 1 << "(mV)";
    }
    for (int slot = 0; slot < selected_count && slot < MAX_MARKER_NUM; ++slot)
    {
        os << ",M" << slot + 1 << "_ID";
        os << ",M" << slot + 1 << "_X(mm)";
        os << ",M" << slot + 1 << "_Y(mm)";
        os << ",M" << slot + 1 << "_Z(mm)";
    }
    os << "\n";

    for (const auto& row : records)
    {
        os << row.sample_time_s;
        for (int device = 0; device < OPT_SAMPLING_BLE_DEVICE_NUM; ++device)
        {
            const auto& ble = row.ble[device];
            for (int channel = 0; channel < OPT_SAMPLING_BLE_CHANNEL_NUM; ++channel)
            {
                os << "," << ble.channel_mv[channel];
            }
        }

        for (int slot = 0; slot < selected_count && slot < MAX_MARKER_NUM; ++slot)
        {
            if (row.marker_valid[slot])
            {
                os << "," << row.markers[slot].ID
                    << "," << row.markers[slot].XYZ[0]
                    << "," << row.markers[slot].XYZ[1]
                    << "," << row.markers[slot].XYZ[2];
            }
            else
            {
                os << ",,,,";
            }
        }
        os << "\n";
    }

    os.close();
    UI_INFO(u8"保存Opt采样数据%llu行到文件%s成功！", static_cast<unsigned long long>(records.size()), filename);
}
