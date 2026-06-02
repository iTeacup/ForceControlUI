#pragma once

#include "UIBaseWindow.h"
#include "UIPlotDef.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define BLE_STRAIN_CHANNEL_NUM 4

struct ST_BleStrainData
{
    float channel_mv[BLE_STRAIN_CHANNEL_NUM] = { 0.0f };
};

class UIBLEMonitor : public UIBaseWindow
{
public:
    UIBLEMonitor(UIMainWindowBase* main_win, const char* title);
    ~UIBLEMonitor();

    void Draw() override;
    EUIMenuCategory GetWinMenuCategory() override { return EUIMenuCategory::E_UI_CAT_APP; }
    const char* GetShowShortCut() override { return "Ctrl+5"; }

private:
    enum class EBleState
    {
        Disconnected,
        Scanning,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };

    struct SBleDeviceMonitor
    {
        uint32_t id = 0;
        char display_name[64] = "BLE设备";
        char device_name[128] = "nRF52840_Strain";
        char service_uuid[64] = "19B10000-E8F2-537E-4F6C-D104768A1214";
        char characteristic_uuid[64] = "19B10002-E8F2-537E-4F6C-D104768A1214";

        mutable std::mutex state_lock;
        EBleState state = EBleState::Disconnected;
        std::string state_msg = u8"未连接";
        std::string last_packet;
        uint64_t packet_count = 0;

        std::atomic_bool stop_ble = false;
        std::thread ble_thread;

        ST_BleStrainData current_data;
        std::mutex data_lock;

        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last_packet_time = std::chrono::steady_clock::now();
        std::mutex hist_lock;
        std::vector<ScrollingBuffer> hist_data;

        std::atomic_bool start_serialize = false;
        std::atomic_bool serialize_run_flag = true;
        std::thread serialize_thread;
        std::mutex record_lock;
        std::vector<ST_BleStrainData> record_data;
    };

    void AddDevice();
    void RemoveDevice(size_t index);
    bool IsBusyOrConnected(const SBleDeviceMonitor& device) const;
    bool IsConnected(const SBleDeviceMonitor& device) const;
    void StartConnect(SBleDeviceMonitor* device);
    void StopConnect(SBleDeviceMonitor* device);
    void StopAllDevices();
    void StartSerializeAllConnected();
    void StopSerializeAll();
    void BleThreadFunc(SBleDeviceMonitor* device);
    void SerializeThreadFunc(SBleDeviceMonitor* device);
    void OnPacketReceived(SBleDeviceMonitor* device, const std::string& packet);
    bool ParsePacket(const std::string& packet, ST_BleStrainData& data);
    void SetState(SBleDeviceMonitor* device, EBleState state, const std::string& message);
    void DrawDevice(size_t index, SBleDeviceMonitor* device);

private:
    static constexpr const char* STRAIN_SERVICE_UUID = "19B10000-E8F2-537E-4F6C-D104768A1214";
    static constexpr const char* STRAIN_CHARACTERISTIC_UUID = "19B10002-E8F2-537E-4F6C-D104768A1214";

    uint32_t m_next_device_id = 1;
    float m_history_seconds = 30.0f;
    std::chrono::steady_clock::time_point m_plot_start_time = std::chrono::steady_clock::now();
    std::vector<std::unique_ptr<SBleDeviceMonitor>> m_devices;
};
