#pragma once

#include "CPSOptServerDef.h"
#include "UIBaseWindow.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define OPT_SAMPLING_BLE_DEVICE_NUM 2
#define OPT_SAMPLING_BLE_CHANNEL_NUM 4
#define OPT_SAMPLING_ADC_CHANNEL_NUM (OPT_SAMPLING_BLE_DEVICE_NUM * OPT_SAMPLING_BLE_CHANNEL_NUM)

struct ST_OptSamplingBleData
{
    uint32_t timestamp = 0;
    float channel_mv[OPT_SAMPLING_BLE_CHANNEL_NUM] = { 0.0f };
    double app_time_s = 0.0;
    uint64_t packet_index = 0;
    bool valid = false;
};

class UIOptSampling : public UIBaseWindow
{
public:
    UIOptSampling(UIMainWindowBase* main_win, const char* title);
    ~UIOptSampling();

    void Draw() override;
    EUIMenuCategory GetWinMenuCategory() override { return EUIMenuCategory::E_UI_CAT_APP; }
    const char* GetShowShortCut() override { return "Ctrl+7"; }

    void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len) override;

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

    struct SBleOptDevice
    {
        uint32_t id = 0;
        char display_name[64] = "BLE设备";
        char device_name[128] = "nRF52840_01";
        char service_uuid[64] = "19B10000-E8F2-537E-4F6C-D104768A1214";
        char characteristic_uuid[64] = "19B10002-E8F2-537E-4F6C-D104768A1214";

        mutable std::mutex state_lock;
        EBleState state = EBleState::Disconnected;
        std::string state_msg = u8"未连接";
        std::string last_packet_text;
        uint64_t packet_count = 0;

        std::atomic_bool stop_ble = false;
        std::thread ble_thread;

        mutable std::mutex data_lock;
        ST_OptSamplingBleData current_data;
    };

    struct SRecordRow
    {
        double sample_time_s = 0.0;
        ST_OptSamplingBleData ble[OPT_SAMPLING_BLE_DEVICE_NUM];
        ST_OptMarker markers[MAX_MARKER_NUM] = {};
        bool marker_valid[MAX_MARKER_NUM] = {};
    };

    void DrawTitle();
    void DrawBleConfig();
    void DrawBleDevice(size_t index, SBleOptDevice* device);
    void DrawOptConfig();
    void DrawSamplingControl();
    void DrawRecordTable();

    void InitBleDevices();
    bool IsBleBusyOrConnected(const SBleOptDevice& device) const;
    bool IsBleConnected(const SBleOptDevice& device) const;
    bool AreAllBleConnected() const;
    bool IsAnyBleConnected() const;
    void StartBleConnect(SBleOptDevice* device);
    void StopBleConnect(SBleOptDevice* device);
    void StopAllBleDevices();
    void BleThreadFunc(SBleOptDevice* device);
    void OnBlePacketReceived(SBleOptDevice* device, const std::string& packet);
    bool ParseBlePacket(const std::string& packet, ST_OptSamplingBleData& data);
    void SetBleState(SBleOptDevice* device, EBleState state, const std::string& message);
    void CopyBleData(ST_OptSamplingBleData ble[OPT_SAMPLING_BLE_DEVICE_NUM]);

    void InitOptService();
    void StopOptService();
    void OnInitRsp(const char* data, uint32_t msg_len);
    void OnStopRsp(const char* data, uint32_t msg_len);
    void OnMarkerListMsg(const char* data, uint32_t msg_len);
    int GetIndexByID(int id);

    void StartRecording();
    void StopRecording();
    void ResetRecords();
    void SaveRecords();
    void SamplingThreadFunc(float interval_s);
    void CaptureSample(double sample_time_s);
    double NowSeconds() const;

private:
    std::array<SBleOptDevice, OPT_SAMPLING_BLE_DEVICE_NUM> m_ble_devices;

    std::mutex m_marker_list_lock;
    ST_OptMarker_List m_marker_list = { 0 };
    std::mutex m_selection_lock;
    int m_disp_marker_count = 6;
    std::vector<int> m_selected_ids;
    float m_sample_interval_s = 0.1f;

    std::atomic_bool m_recording = false;
    std::thread m_sampling_thread;
    std::chrono::steady_clock::time_point m_time_zero = std::chrono::steady_clock::now();
    std::mutex m_record_lock;
    std::vector<SRecordRow> m_records;
    size_t m_record_table_auto_scroll_count = 0;

    int m_req_id = 0;
};
