#pragma once
#include <CPSNokovDef.h>
#include <array>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace sync_capture
{
double HostSeconds();
using Names = std::map<int, std::pair<std::string, std::string>>;
enum class Basis
{
    Ble,
    Nokov,
    Fixed
};
struct Options
{
    Basis basis = Basis::Ble;
    double frequency = 100, maxGap = 0.1, bleOffset = 0;
};
struct ExportResult
{
    uint64_t rows = 0, excluded = 0;
};
// Re-reads original records, so changing output settings never compounds interpolation.
ExportResult Export(const std::filesystem::path &directory, const Options &options);

class Capture
{
  public:
    enum class Phase
    {
        Idle,
        Recording,
        Finishing
    };
    struct View
    {
        Phase phase = Phase::Idle;
        bool bleConnected = false, bleReady = false, nokovReady = false;
        double bleHz = 0, nokovHz = 0;
        uint64_t bleWritten = 0, nokovWritten = 0, rows = 0, completed = 0;
        std::string path, error, message;
        Names names;
        nokov::Frame preview;
        std::array<float, 4> channels{};
    };
    Capture();
    ~Capture();
    void SelectBle(uint32_t id);
    void BleState(uint32_t id, uint64_t generation, bool connected);
    void Ble(uint32_t id, uint64_t generation, uint32_t timestamp, const float *channels, double host);
    void Nokov(nokov::Frame frame, double host);
    void Status(const nokov::Status &status);
    void Fail(const std::string &reason);
    bool Start(const std::filesystem::path &root, const std::set<int> &selected, const Options &options);
    void Stop();
    bool Reexport(const Options &options, const std::filesystem::path &directory = {});
    View Snapshot();
    void Close();

  private:
    struct Event
    {
        bool ble = false;
        double host = 0;
        uint64_t timestamp = 0;
        std::array<float, 4> channels{};
        nokov::Frame frame;
        size_t Bytes() const
        {
            return sizeof(Event) + frame.markers.size() * sizeof(nokov::Marker);
        }
    };
    void FailLocked(const std::string &reason);
    void Push(Event event);
    void Run();
    bool Ready(double now) const;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    bool m_close = false, m_new = false, m_exportOnly = false;
    View m_view;
    uint32_t m_bleId = 1, m_bleRaw = 0;
    uint64_t m_generation = 0, m_bleExtended = 0;
    double m_bleHost = 0, m_nokovHost = 0, m_previewHost = 0;
    double m_bleRateStart = 0, m_nokovRateStart = 0;
    uint64_t m_bleRateCount = 0, m_nokovRateCount = 0;
    bool m_haveBle = false, m_haveNokov = false;
    nokov::Header m_lastHeader;
    nokov::Status m_status;
    Names m_selected;
    std::map<std::pair<std::string, std::string>, int> m_selectedNames;
    Options m_options;
    double m_origin = 0;
    std::deque<Event> m_queue;
    size_t m_queueBytes = 0;
};
} // namespace sync_capture
