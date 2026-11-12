#include "SyncCapture.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace sync_capture
{
using Json = nlohmann::json;
namespace
{
constexpr size_t MaxQueue = 8192, MaxQueueBytes = 64 * 1024 * 1024;
constexpr double StaleSeconds = 2;
void CheckOptions(const Options &options)
{
    if (options.basis != Basis::Ble && options.basis != Basis::Nokov && options.basis != Basis::Fixed)
        throw std::runtime_error(u8"未知输出基准");
    if (!std::isfinite(options.frequency) || options.frequency < 1 || options.frequency > 2000 ||
        !std::isfinite(options.maxGap) || options.maxGap <= 0 || options.maxGap > 10 ||
        !std::isfinite(options.bleOffset) || std::abs(options.bleOffset) > 60)
        throw std::runtime_error(u8"输出参数无效：输出 Hz 为 1～2000，插值间隔为 0～10 秒，偏移不超过 60 秒");
}
std::string Quote(const std::string &text)
{
    std::string out = "\"";
    for (char c : text)
    {
        out += c;
        if (c == '"')
            out += c;
    }
    return out + '"';
}
Json ReadJson(const std::filesystem::path &path)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error(u8"无法读取采集文件");
    Json json;
    input >> json;
    return json;
}
void ReplaceFile(const std::filesystem::path &source, const std::filesystem::path &destination)
{
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error(u8"输出文件替换失败，请关闭占用该文件的程序");
#else
    std::filesystem::rename(source, destination);
#endif
}
void WriteJson(const std::filesystem::path &path, const Json &json)
{
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream out(temporary, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << json.dump(2) << '\n';
    out.close();
    ReplaceFile(temporary, path);
}
struct Reader
{
    std::ifstream file;
    explicit Reader(const std::filesystem::path &path) : file(path)
    {
        if (!file)
            throw std::runtime_error(u8"原始记录打开失败");
    }
    bool Next(Json &value)
    {
        std::string line;
        if (std::getline(file, line))
        {
            value = Json::parse(line);
            return true;
        }
        if (!file.eof())
            throw std::runtime_error(u8"原始记录读取失败");
        return false;
    }
};
// One low-delay representative per source second bounds fitting memory for long captures.
// This estimates a receive-time mapping; unknown fixed transport delay remains uncalibrated.
struct Fit
{
    int64_t origin = 0;
    double a = 1, b = 0, span = 0, residualP95 = 0;
    uint64_t count = 0;
    bool driftEstimated = false;
    double At(int64_t timestamp) const
    {
        return a * (timestamp - origin) * 0.001 + b;
    }
    Json Description() const
    {
        return {{"source_origin_ms", origin},
                {"scale", a},
                {"offset_s", b},
                {"span_s", span},
                {"samples", count},
                {"drift_estimated", driftEstimated},
                {"receive_residual_p95_s", residualP95}};
    }
};
Fit Estimate(const std::filesystem::path &path)
{
    Reader reader(path);
    Json row;
    Fit fit;
    std::vector<std::pair<double, double>> anchors;
    int64_t last = 0, bin = -1;
    while (reader.Next(row))
    {
        const int64_t stamp = row.at("source_ms").get<int64_t>();
        const double host = row.at("host_s").get<double>();
        if (!std::isfinite(host) || (fit.count && stamp <= last))
            throw std::runtime_error(u8"原始记录时间不递增，不能生成对齐表");
        if (!fit.count)
            fit.origin = stamp;
        last = stamp;
        const double x = (stamp - fit.origin) * 0.001;
        const auto nextBin = static_cast<int64_t>(std::floor(x));
        if (nextBin != bin)
        {
            anchors.emplace_back(x, host);
            bin = nextBin;
        }
        else if (host - x < anchors.back().second - anchors.back().first)
            anchors.back() = {x, host};
        fit.span = x;
        ++fit.count;
    }
    if (fit.count < 2 || fit.span <= 0)
        throw std::runtime_error(u8"两路都需要至少两条不同时间的有效数据");
    if (fit.span >= 10 && anchors.size() >= 8)
    {
        double mx = 0, my = 0, xx = 0, xy = 0;
        for (const auto &p : anchors)
        {
            mx += p.first;
            my += p.second;
        }
        mx /= anchors.size();
        my /= anchors.size();
        for (const auto &p : anchors)
        {
            xx += (p.first - mx) * (p.first - mx);
            xy += (p.first - mx) * (p.second - my);
        }
        fit.a = xy / xx;
        if (!std::isfinite(fit.a) || std::abs(fit.a - 1) > 0.005)
            throw std::runtime_error(u8"设备时间与接收时间走速差异过大，保留原始记录，请检查传输积压或设备时钟");
        fit.driftEstimated = true;
    }
    std::vector<double> offsets;
    for (const auto &p : anchors)
        offsets.push_back(p.second - fit.a * p.first);
    std::sort(offsets.begin(), offsets.end());
    fit.b = offsets[static_cast<size_t>((offsets.size() - 1) * 0.1)];
    fit.residualP95 = offsets[static_cast<size_t>((offsets.size() - 1) * 0.95)] - fit.b;
    return fit;
}
struct Cursor
{
    Reader reader;
    Fit fit;
    Json previous, next;
    double previousTime = 0, nextTime = 0;
    bool hasPrevious = false, hasNext = false;
    Cursor(const std::filesystem::path &path, Fit mapping) : reader(path), fit(mapping)
    {
        Advance();
    }
    void Advance()
    {
        if (hasNext)
        {
            previous = std::move(next);
            previousTime = nextTime;
            hasPrevious = true;
        }
        hasNext = reader.Next(next);
        if (hasNext)
            nextTime = fit.At(next.at("source_ms").get<int64_t>());
    }
    bool Bracket(double t, double maxGap)
    {
        while (hasNext && nextTime < t - 1e-9)
            Advance();
        if (hasNext && std::abs(nextTime - t) < 1e-9)
            return true;
        return hasPrevious && hasNext && t >= previousTime && t <= nextTime && nextTime - previousTime <= maxGap;
    }
    bool Values(double t, double maxGap, bool ble, const Names &names, std::vector<double> &values)
    {
        if (!Bracket(t, maxGap))
            return false;
        const bool exact = std::abs(nextTime - t) < 1e-9;
        const Json &left = exact ? next : previous;
        const double weight = exact ? 0 : (t - previousTime) / (nextTime - previousTime);
        auto append = [&](const Json &lo, const Json &hi, size_t start, size_t n) {
            for (size_t i = start; i < start + n; ++i)
            {
                double x = lo.at(i).get<double>(), y = hi.at(i).get<double>();
                const double result = x + weight * (y - x);
                if (!std::isfinite(result))
                    return false;
                values.push_back(result);
            }
            return true;
        };
        if (ble)
            return append(left.at("channels"), next.at("channels"), 0, 4);
        for (const auto &name : names)
        {
            const Json *lo = nullptr, *hi = nullptr;
            for (const auto &p : left.at("points"))
                if (p.at(0).get<int>() == name.first)
                    lo = &p;
            for (const auto &p : next.at("points"))
                if (p.at(0).get<int>() == name.first)
                    hi = &p;
            if (!lo || !hi || !append(*lo, *hi, 1, 3))
                return false;
        }
        return true;
    }
};
} // namespace

double HostSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

ExportResult Export(const std::filesystem::path &directory, const Options &options)
{
    CheckOptions(options);
    auto manifest = ReadJson(directory / "capture.json");
    if (manifest.at("capture_complete") != true)
        throw std::runtime_error(u8"本轮采集异常结束，原始记录已保留，不生成训练表");
    manifest["dataset_ready"] = false;
    manifest["export_error"] = nullptr;
    WriteJson(directory / "capture.json", manifest);
    Names names;
    for (const auto &point : manifest.at("points"))
        names[point.at("id").get<int>()] = {point.at("asset").get<std::string>(), point.at("name").get<std::string>()};
    Fit bleFit = Estimate(directory / "ble_raw.jsonl"), nokovFit = Estimate(directory / "nokov_raw.jsonl");
    bleFit.b += options.bleOffset;
    Cursor ble(directory / "ble_raw.jsonl", bleFit), nokov(directory / "nokov_raw.jsonl", nokovFit);
    const double begin = std::max(bleFit.b, nokovFit.b);
    const double end = std::min(bleFit.a * bleFit.span + bleFit.b, nokovFit.a * nokovFit.span + nokovFit.b);
    if (end < begin)
        throw std::runtime_error(u8"两路数据没有重叠时间，不能生成对齐表");
    const auto temp = directory / "dataset.csv.tmp";
    ExportResult result;
    try
    {
        std::ofstream out(temp, std::ios::binary);
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out << "\xEF\xBB\xBFtime_s,ch1_mV,ch2_mV,ch3_mV,ch4_mV";
        for (const auto &p : names)
            for (const char *axis : {"x_mm", "y_mm", "z_mm"})
                out << ',' << Quote(p.second.first + "/" + p.second.second + "/" + axis);
        out << '\n' << std::fixed << std::setprecision(9);
        auto emit = [&](double t) {
            if (t < begin - 1e-9 || t > end + 1e-9)
                return;
            std::vector<double> values;
            if (!ble.Values(t, options.maxGap, true, names, values) ||
                !nokov.Values(t, options.maxGap, false, names, values))
            {
                ++result.excluded;
                return;
            }
            out << t;
            for (double value : values)
                out << ',' << value;
            out << '\n';
            ++result.rows;
        };
        if (options.basis == Basis::Fixed)
        {
            const uint64_t count = static_cast<uint64_t>(std::floor((end - begin) * options.frequency)) + 1;
            for (uint64_t i = 0; i < count; ++i)
                emit(begin + i / options.frequency);
        }
        else
        {
            const bool useBle = options.basis == Basis::Ble;
            Reader targets(directory / (useBle ? "ble_raw.jsonl" : "nokov_raw.jsonl"));
            const auto &fit = useBle ? bleFit : nokovFit;
            Json row;
            while (targets.Next(row))
                emit(fit.At(row.at("source_ms").get<int64_t>()));
        }
        out.close();
        // An existing dataset is only replaced after the new file has been fully written.
        ReplaceFile(temp, directory / "dataset.csv");
        manifest["alignment"] = {{"method", "source_clock_fit_to_low_delay_receive_times"},
                                 {"validated", false},
                                 {"accuracy_s", nullptr},
                                 {"fixed_transport_delay_corrected", false},
                                 {"note", u8"初步软件对齐；接收残差不是同步精度；未消除固定传输延迟"},
                                 {"ble", bleFit.Description()},
                                 {"nokov", nokovFit.Description()},
                                 {"ble_time_adjustment_s", options.bleOffset}};
        manifest["output"] = {{"basis", options.basis == Basis::Ble     ? "ble"
                                        : options.basis == Basis::Nokov ? "nokov"
                                                                        : "fixed"},
                              {"frequency_hz", options.basis == Basis::Fixed ? Json(options.frequency) : Json(nullptr)},
                              {"maximum_interpolation_gap_s", options.maxGap},
                              {"rows", result.rows},
                              {"excluded_rows", result.excluded},
                              {"time_origin", "joint_capture_start"},
                              {"coordinate_unit", "mm"},
                              {"voltage_unit", "mV"},
                              {"interpolation", "linear; no extrapolation; no missing-point filling"},
                              {"anti_alias_filter", false}};
        manifest["dataset_ready"] = true;
        WriteJson(directory / "capture.json", manifest);
        return result;
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        throw;
    }
}

Capture::Capture()
{
    m_worker = std::thread(&Capture::Run, this);
}
Capture::~Capture()
{
    Close();
}
void Capture::Close()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_close = true;
        if (m_view.phase == Phase::Recording)
            m_view.phase = Phase::Finishing;
    }
    m_cv.notify_one();
    if (m_worker.joinable())
        m_worker.join();
}
void Capture::SelectBle(uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_view.phase != Phase::Idle || id == m_bleId)
        return;
    m_bleId = id;
    m_haveBle = false;
    m_view.bleConnected = false;
    m_view.bleHz = 0;
    m_bleRateCount = 0;
}
void Capture::BleState(uint32_t id, uint64_t generation, bool connected)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (id != m_bleId)
        return;
    if (!connected || (m_haveBle && generation != m_generation))
    {
        FailLocked(u8"BLE 已断开或重新连接，本轮采集结束");
        m_haveBle = false;
    }
    m_view.bleConnected = connected;
}
void Capture::FailLocked(const std::string &reason)
{
    if (m_view.phase != Phase::Recording)
        return;
    m_view.error = reason;
    m_view.phase = Phase::Finishing;
    m_cv.notify_one();
}
void Capture::Fail(const std::string &reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    FailLocked(reason);
}
void Capture::Push(Event event)
{
    if (m_view.phase != Phase::Recording || event.host < m_origin)
        return;
    if (m_queue.size() >= MaxQueue || event.Bytes() > MaxQueueBytes - m_queueBytes)
    {
        FailLocked(u8"联合采集写入队列已满，原始记录可能不完整");
        return;
    }
    m_queueBytes += event.Bytes();
    m_queue.push_back(std::move(event));
    m_cv.notify_one();
}
void Capture::Ble(uint32_t id, uint64_t generation, uint32_t timestamp, const float *channels, double host)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (id != m_bleId)
        return;
    for (int i = 0; i < 4; ++i)
        if (!std::isfinite(channels[i]))
        {
            FailLocked(u8"BLE 包含无效电压");
            return;
        }
    if (m_haveBle && generation != m_generation)
    {
        FailLocked(u8"BLE 连接已变化");
        m_haveBle = false;
    }
    if (m_haveBle)
    {
        const uint32_t delta = timestamp - m_bleRaw;
        // Unsigned subtraction accepts the normal millis() rollover, but not resets/reordered packets.
        if (!delta || delta > 0x7FFFFFFFu)
        {
            FailLocked(u8"BLE 时间戳重复、回退或板子重启");
            m_haveBle = false;
            return;
        }
        m_bleExtended += delta;
    }
    else
        m_bleExtended = timestamp;
    m_bleRaw = timestamp;
    m_generation = generation;
    m_haveBle = true;
    m_bleHost = host;
    std::copy(channels, channels + 4, m_view.channels.begin());
    if (!m_bleRateCount)
        m_bleRateStart = host;
    ++m_bleRateCount;
    if (host - m_bleRateStart >= 1)
    {
        m_view.bleHz = (m_bleRateCount - 1) / (host - m_bleRateStart);
        m_bleRateCount = 0;
    }
    Event event;
    event.ble = true;
    event.host = host;
    event.timestamp = m_bleExtended;
    event.channels = m_view.channels;
    Push(std::move(event));
}
void Capture::Nokov(nokov::Frame frame, double host)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frame.markers.size() > nokov::MaxMarkers || frame.header.timestamp <= 0)
    {
        FailLocked(u8"NOKOV 帧大小或时间戳无效");
        return;
    }
    if (m_haveNokov && frame.header.session != m_lastHeader.session)
    {
        FailLocked(u8"NOKOV 会话已变化，请重新采集");
        m_view.names.clear();
        m_view.preview = {};
        m_previewHost = 0;
        m_nokovRateCount = 0;
    }
    if (m_haveNokov &&
        (frame.header.sequence != m_lastHeader.sequence + 1 || frame.header.timestamp <= m_lastHeader.timestamp ||
         static_cast<uint32_t>(frame.header.frame) - static_cast<uint32_t>(m_lastHeader.frame) != 1))
        FailLocked(u8"NOKOV 帧序号中断或时间戳不递增");
    std::set<int> ids;
    std::set<std::pair<std::string, std::string>> identities;
    for (const auto &p : frame.markers)
    {
        const std::pair<std::string, std::string> name{p.asset, p.name};
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !ids.insert(p.id).second)
        {
            FailLocked(u8"NOKOV 包含无效坐标或重复点");
            return;
        }
        if (p.asset[0] && p.name[0])
            m_view.names[p.id] = name;
        else
            m_view.names.erase(p.id);
        if (m_view.phase == Phase::Recording && m_selected.count(p.id) && m_selected.at(p.id) != name)
            FailLocked(u8"所选动捕点的名称对应关系已改变");
        if (!identities.insert(name).second && p.asset[0] && p.name[0])
            FailLocked(u8"动捕点名称不唯一");
        const auto chosen = m_selectedNames.find(name);
        if (m_view.phase == Phase::Recording && chosen != m_selectedNames.end() && chosen->second != p.id)
            FailLocked(u8"动捕点身份已变化");
    }
    m_haveNokov = true;
    m_lastHeader = frame.header;
    m_nokovHost = host;
    if (!m_nokovRateCount)
        m_nokovRateStart = host;
    ++m_nokovRateCount;
    if (host - m_nokovRateStart >= 1)
    {
        m_view.nokovHz = (m_nokovRateCount - 1) / (host - m_nokovRateStart);
        m_nokovRateCount = 0;
    }
    if (host - m_previewHost >= 0.1)
    {
        m_view.preview = frame;
        m_previewHost = host;
    }
    Event event;
    event.frame = std::move(frame);
    event.host = host;
    Push(std::move(event));
}
void Capture::Status(const nokov::Status &status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!status.connected || status.version != nokov::Version || status.overflow > m_status.overflow ||
        status.failed > m_status.failed || status.invalidFrames > m_status.invalidFrames ||
        status.sdkGaps > m_status.sdkGaps)
        FailLocked(u8"NOKOV 已断开或服务报告数据链路异常");
    m_status = status;
}
bool Capture::Ready(double now) const
{
    return m_haveBle && m_haveNokov && m_view.bleConnected && m_status.connected &&
           m_status.version == nokov::Version && now - m_bleHost < StaleSeconds && now - m_nokovHost < StaleSeconds;
}
bool Capture::Start(const std::filesystem::path &root, const std::set<int> &selected, const Options &options)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_view.phase != Phase::Idle || m_close)
        return false;
    try
    {
        CheckOptions(options);
        if (!Ready(HostSeconds()))
            throw std::runtime_error(u8"请等待 BLE 和 NOKOV 都连接并收到有效数据");
        if (selected.empty())
            throw std::runtime_error(u8"请至少选择一个标记点");
        m_selected.clear();
        m_selectedNames.clear();
        std::set<std::pair<std::string, std::string>> unique;
        for (int id : selected)
        {
            auto name = m_view.names.find(id);
            if (name == m_view.names.end() || !unique.insert(name->second).second)
                throw std::runtime_error(u8"所选点名称未知或不唯一");
            m_selected.insert(*name);
            m_selectedNames[name->second] = id;
        }
        std::filesystem::create_directories(root);
        const auto path = root / ("SYNC_" + std::to_string(nokov::NewLocalId()));
        if (!std::filesystem::create_directory(path))
            throw std::runtime_error(u8"采集目录已存在");
        m_view.path = path.u8string();
        m_view.error.clear();
        m_view.message = u8"正在联合采集";
        m_view.bleWritten = m_view.nokovWritten = m_view.rows = 0;
        m_options = options;
        m_origin = HostSeconds();
        m_queue.clear();
        m_queueBytes = 0;
        m_new = true;
        m_exportOnly = false;
        m_view.phase = Phase::Recording;
        m_cv.notify_one();
        return true;
    }
    catch (const std::exception &error)
    {
        m_view.error = error.what();
        return false;
    }
}
void Capture::Stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_view.phase == Phase::Recording)
    {
        m_view.phase = Phase::Finishing;
        m_cv.notify_one();
    }
}
bool Capture::Reexport(const Options &options, const std::filesystem::path &directory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_view.phase != Phase::Idle || (m_view.path.empty() && directory.empty()) || m_close)
        return false;
    try
    {
        CheckOptions(options);
        if (!directory.empty())
        {
            const auto manifest = ReadJson(directory / "capture.json");
            if (manifest.at("capture_complete") != true)
                throw std::runtime_error(u8"该目录的采集未正常完成，请检查原始记录");
            m_view.path = directory.u8string();
            m_view.bleWritten = manifest.value("ble_samples", uint64_t{0});
            m_view.nokovWritten = manifest.value("nokov_frames", uint64_t{0});
        }
    }
    catch (const std::exception &error)
    {
        m_view.error = error.what();
        return false;
    }
    m_options = options;
    m_view.error.clear();
    m_view.phase = Phase::Finishing;
    m_exportOnly = true;
    m_cv.notify_one();
    return true;
}
Capture::View Capture::Snapshot()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const double now = HostSeconds();
    m_view.bleReady = m_haveBle && m_view.bleConnected && now - m_bleHost < StaleSeconds;
    m_view.nokovReady =
        m_haveNokov && m_status.connected && m_status.version == nokov::Version && now - m_nokovHost < StaleSeconds;
    View view = m_view;
    if (!view.bleReady)
        view.bleHz = 0;
    if (!view.nokovReady)
        view.nokovHz = 0;
    return view;
}
void Capture::Run()
{
    std::ofstream ble, nokov;
    std::unique_lock<std::mutex> lock(m_mutex);
    while (true)
    {
        m_cv.wait_for(lock, std::chrono::milliseconds(100),
                      [&] { return m_close || m_new || !m_queue.empty() || m_view.phase == Phase::Finishing; });
        if (m_view.phase == Phase::Recording && !Ready(HostSeconds()))
            FailLocked(u8"一路数据超过 2 秒未到达，本轮采集结束");
        if (m_new)
        {
            m_new = false;
            try
            {
                const auto path = std::filesystem::u8path(m_view.path);
                ble.open(path / "ble_raw.jsonl", std::ios::binary);
                nokov.open(path / "nokov_raw.jsonl", std::ios::binary);
                ble.exceptions(std::ios::badbit | std::ios::failbit);
                nokov.exceptions(std::ios::badbit | std::ios::failbit);
                Json points = Json::array();
                for (const auto &p : m_selected)
                    points.push_back({{"id", p.first}, {"asset", p.second.first}, {"name", p.second.second}});
                WriteJson(path / "capture.json", {{"version", 1},
                                                  {"capture_complete", false},
                                                  {"ble_device_id", m_bleId},
                                                  {"ble_connection_generation", m_generation},
                                                  {"nokov_session", m_lastHeader.session},
                                                  {"points", points},
                                                  {"host_clock", "same_process_steady_clock"},
                                                  {"host_origin_s", m_origin},
                                                  {"source_time_unit", "ms"},
                                                  {"host_time_unit", "s"}});
            }
            catch (const std::exception &error)
            {
                FailLocked(std::string(u8"原始文件创建失败：") + error.what());
            }
        }
        while (!m_queue.empty())
        {
            Event event = std::move(m_queue.front());
            m_queue.pop_front();
            m_queueBytes -= event.Bytes();
            const double origin = m_origin;
            lock.unlock();
            bool ok = true;
            try
            {
                if (event.ble)
                    ble << Json({{"source_ms", event.timestamp},
                                 {"timestamp_raw_ms", static_cast<uint32_t>(event.timestamp)},
                                 {"host_s", event.host - origin},
                                 {"channels", event.channels}})
                               .dump()
                        << '\n';
                else
                {
                    Json points = Json::array();
                    for (const auto &p : event.frame.markers)
                        if (m_selected.count(p.id))
                            points.push_back({p.id, p.x, p.y, p.z});
                    nokov << Json({{"source_ms", event.frame.header.timestamp},
                                   {"host_s", event.host - origin},
                                   {"frame", event.frame.header.frame},
                                   {"sequence", event.frame.header.sequence},
                                   {"points", points}})
                                 .dump()
                          << '\n';
                }
            }
            catch (const std::exception &)
            {
                ok = false;
            }
            lock.lock();
            if (!ok)
            {
                m_view.error = u8"原始记录写入失败，可能磁盘已满";
                m_view.phase = Phase::Finishing;
                m_queue.clear();
                m_queueBytes = 0;
                break;
            }
            if (event.ble)
                ++m_view.bleWritten;
            else
                ++m_view.nokovWritten;
        }
        if (m_view.phase == Phase::Finishing)
        {
            const auto path = std::filesystem::u8path(m_view.path);
            const auto options = m_options;
            const bool exportOnly = m_exportOnly;
            std::string error = m_view.error;
            const auto bleCount = m_view.bleWritten, nokovCount = m_view.nokovWritten;
            lock.unlock();
            ExportResult result;
            try
            {
                if (!exportOnly)
                {
                    if (ble.is_open())
                        ble.close();
                    if (nokov.is_open())
                        nokov.close();
                    auto manifest = ReadJson(path / "capture.json");
                    manifest["capture_complete"] = error.empty();
                    manifest["capture_error"] = error;
                    manifest["ble_samples"] = bleCount;
                    manifest["nokov_frames"] = nokovCount;
                    WriteJson(path / "capture.json", manifest);
                }
                if (error.empty())
                    result = Export(path, options);
            }
            catch (const std::exception &exception)
            {
                if (error.empty())
                    error = exception.what();
                try
                {
                    auto manifest = ReadJson(path / "capture.json");
                    manifest["dataset_ready"] = false;
                    manifest["export_error"] = error;
                    WriteJson(path / "capture.json", manifest);
                }
                catch (...)
                { /* Preserve the original I/O error if the disk is unavailable. */
                }
            }
            // Clear stream error flags so a failed capture cannot poison the next one.
            ble.exceptions(std::ios::goodbit);
            nokov.exceptions(std::ios::goodbit);
            if (ble.is_open())
                ble.close();
            if (nokov.is_open())
                nokov.close();
            ble.clear();
            nokov.clear();
            lock.lock();
            m_view.error = error;
            m_view.rows = result.rows;
            m_view.message = error.empty() ? (result.rows ? u8"已生成初步对齐数据集，精度待验证"
                                                          : u8"有效配对为 0，请检查遮挡、偏移和插值间隔")
                                           : u8"原始记录已保留，请检查错误信息";
            m_view.phase = Phase::Idle;
            ++m_view.completed;
            m_exportOnly = false;
        }
        if (m_close && m_view.phase == Phase::Idle)
            break;
    }
}
} // namespace sync_capture
