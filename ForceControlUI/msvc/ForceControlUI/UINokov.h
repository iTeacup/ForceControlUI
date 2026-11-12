#pragma once
#include "UIBaseWindow.h"
#include <CPSNokovDef.h>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>
#include <set>
#include <map>
#include <fstream>
#include <optional>
class UINokov : public UIBaseWindow
{
public:
    UINokov(UIMainWindowBase*, const char*);
    ~UINokov() override;
    EUIMenuCategory GetWinMenuCategory() override { return EUIMenuCategory::E_UI_CAT_APP; }
    void Draw() override;
    void OnCPSMsg(uint32_t, uint32_t, const char*, uint32_t) override;

private:
    void WriteFrames();
    void FinishCapture();
    void DrawPoints(float tableHeight = 0);
    void DrawSettings(bool online);
    void DrawCaptureControl();
    void DrawPointSelection(int id, bool named);
    void StartCapture(bool single = false);
    void StopCapture();
    void ReportError(const std::string& message);
    using Names = std::map<int, std::pair<std::string, std::string>>;
    void SendCommand(bool connect);

    using Clock = std::chrono::steady_clock;
    struct RuntimeTiming
    {
        Clock::time_point last_frame{}, last_status{}, preview_sample{}, rate_sample{};
        uint64_t rate_frames = 0;
    };

    Names m_known_names, m_recorded_names;
    uint64_t m_recording_session = 0;
    std::optional<int64_t> m_timestamp_origin; // SDK milliseconds of the first saved frame.

    std::mutex m_mutex;
    std::condition_variable m_write_cv;
    std::thread m_writer_thread;
    bool m_connection_requested = false, m_awaiting_connection = false, m_awaiting_disconnection = false;
    bool m_single_capture = false;
    double m_receive_hz = 0;
    RuntimeTiming m_timing;
    bool m_exiting = false, m_recording = false, m_finishing = false;
    bool m_incomplete = false, m_has_frame = false, m_record_all = true, m_disconnect_pending = false;
    char m_save_directory[512] = "data/nokov";
    std::string m_recording_path, m_error;
    std::set<int> m_selected_ids, m_recorded_ids;
    std::deque<nokov::Frame> m_pending_frames;
    nokov::Frame m_latest_frame, m_preview_frame;
    nokov::Status m_status;
    uint64_t m_received_frames = 0, m_accepted_frames = 0, m_written_frames = 0;
    std::ofstream m_data_file;
};
