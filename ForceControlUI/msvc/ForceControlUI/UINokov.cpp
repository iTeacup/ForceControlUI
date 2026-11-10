#include "UINokov.h"
#include "UIApplication.h"
#include "UILog.h"
#include <CPSAPI/CPSAPI.h>
#include <imgui.h>
#include "IconsFontAwesome6.h"
#include <filesystem>
#include <iomanip>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace
{
using PointNames = std::map<int, std::pair<std::string, std::string>>;
constexpr const char* CsvHeader = "frame,time_s,row_type,asset_name,marker_name,present,x,y,z\n";
std::string CsvField(const std::string& value)
{
    std::string result = "\"";
    for (char c : value)
    {
        if (c == '"')
            result += '"';
        result += c;
    }
    return result + '"';
}
void WriteCsvRows(std::ostream& out, const nokov::Frame& frame, const std::set<int>& ids, const PointNames& names,
                  int64_t timestampBase)
{
    if (frame.header.timestamp < timestampBase)
    {
        out.setstate(std::ios::failbit);
        return;
    }
    // Validate before writing the frame row; unknown identities must not look valid.
    for (const auto& marker : frame.markers)
        if ((ids.empty() || ids.count(marker.id)) && (!marker.asset[0] || !marker.name[0]))
        {
            out.setstate(std::ios::failbit);
            return;
        }
    for (int id : ids)
    {
        auto found = names.find(id);
        if (found == names.end() || found->second.first.empty() || found->second.second.empty())
        {
            out.setstate(std::ios::failbit);
            return;
        }
    }
    // Subtract integers before formatting so large absolute timestamps retain millisecond precision.
    const uint64_t elapsed = static_cast<uint64_t>(frame.header.timestamp) - static_cast<uint64_t>(timestampBase);
    std::string fraction = std::to_string(elapsed % 1000);
    const std::string relative =
        std::to_string(elapsed / 1000) + "." + std::string(3 - fraction.size(), '0') + fraction;
    auto prefix = [&] { out << frame.header.frame << ',' << relative << ','; };
    prefix();
    out << "frame,,,,,,\n";
    for (const auto& marker : frame.markers)
        if (ids.empty() || ids.count(marker.id))
        {
            prefix();
            out << "marker," << CsvField(marker.asset) << ',' << CsvField(marker.name) << ",1," << marker.x << ','
                << marker.y << ',' << marker.z << '\n';
        }
    for (int id : ids)
    {
        bool present = false;
        for (const auto& marker : frame.markers)
            if (marker.id == id)
                present = true;
        if (present)
            continue;
        const auto& name = names.at(id);
        prefix();
        out << "marker," << CsvField(name.first) << ',' << CsvField(name.second) << ",0,,,\n";
    }
}
} // namespace

namespace
{
bool NokovButton(const char* icon, const char* text)
{
    char label[128];
    std::snprintf(label, sizeof(label), "%s %s", icon, text);
    return ImGui::Button(label);
}
} // namespace
UINokov::UINokov(UIMainWindowBase* w, const char* title) : UIBaseWindow(w, title)
{
    m_writer_thread = std::thread(&UINokov::WriteFrames, this);
}
UINokov::~UINokov()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_recording = false;
        m_finishing = true;
        m_exiting = true;
    }
    m_write_cv.notify_one();
    m_writer_thread.join();
}
void UINokov::SendCommand(bool connect)
{
    if (connect && (m_recording || m_finishing))
    {
        m_error = u8"请先结束当前采集再连接NOKOV";
        UI_WARN(u8"[NOKOV] %s", m_error.c_str());
        return;
    }
    auto api = g_app.GetCPSApi();
    if (!api || !api->IsDeviceOnline(nokov::Device))
    {
        m_error = u8"NOKOV 服务未在线，请先启动服务";
        UI_WARN(u8"[NOKOV] %s", m_error.c_str());
        return;
    }
    nokov::Control command;
    command.connect = connect;
    if (api->SendAPPMsg(nokov::Device, nokov::ControlMessage, reinterpret_cast<const char*>(&command), sizeof(command)))
    {
        m_error = u8"NOKOV 命令发送失败";
        UI_ERROR(u8"[NOKOV] %s", m_error.c_str());
        return;
    }
    m_connection_requested = connect;
    m_error.clear();
    m_awaiting_connection = connect;
    m_awaiting_disconnection = !connect;
    if (connect)
        UI_INFO(u8"[NOKOV] 已发送连接请求，正在连接NOKOV……");
    else
        UI_INFO(u8"[NOKOV] 已发送断开请求，正在断开NOKOV……");
    m_receive_hz = 0;
    m_timing.rate_sample = Clock::now();
    m_timing.rate_frames = m_received_frames;
    m_known_names.clear();
    m_selected_ids.clear();
    m_has_frame = false;
    m_latest_frame = {};
    m_preview_frame = {};
    m_timing.last_frame = {};
    m_timing.preview_sample = {};
}
void UINokov::StopCapture()
{
    if (m_recording)
        UI_INFO(u8"[NOKOV] 已停止接收本轮采集数据，正在完成写入……");
    m_recording = false;
    m_finishing = true;
    m_write_cv.notify_one();
}
void UINokov::ReportError(const std::string& message)
{
    if (m_error != message)
        UI_ERROR(u8"[NOKOV] %s", message.c_str());
    m_error = message;
    if (m_recording || m_finishing)
    {
        m_incomplete = true;
        StopCapture();
    }
}
void UINokov::OnCPSMsg(uint32_t from, uint32_t type, const char* bytes, uint32_t size)
{
    if (from != nokov::Device || !bytes)
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (type == nokov::StatusMessage)
    {
        if (size != sizeof(m_status))
        {
            ReportError(u8"协议不匹配，请同时更新服务与上位机");
            return;
        }
        nokov::Status next;
        memcpy(&next, bytes, size);
        if (next.version != nokov::Version)
        {
            ReportError(u8"协议不匹配，请同时更新服务与上位机");
            return;
        }
        if (next.overflow > m_status.overflow || next.failed > m_status.failed ||
            next.invalidFrames > m_status.invalidFrames || next.sdkGaps > m_status.sdkGaps)
            ReportError(u8"动捕数据链路异常，本轮记录可能不完整，请检查连接后重新记录");
        if (m_connection_requested && next.connected && (m_awaiting_connection || !m_status.connected))
        {
            UI_INFO(u8"[NOKOV] NOKOV连接成功");
            m_awaiting_connection = false;
        }
        if (m_awaiting_disconnection && !next.connected)
        {
            UI_INFO(u8"[NOKOV] NOKOV已断开");
            m_awaiting_disconnection = false;
        }
        else if (m_connection_requested && m_status.connected && !next.connected)
        {
            UI_WARN(u8"[NOKOV] NOKOV连接已断开");
        }
        if (m_connection_requested && !next.connected && next.error && next.error != m_status.error)
            UI_ERROR(u8"[NOKOV] NOKOV连接失败，错误码：%d", next.error);
        m_status = next;
        m_timing.last_status = Clock::now();
        return;
    }
    if (type != nokov::FrameMessage || !m_connection_requested)
        return;
    nokov::Frame frame;
    if (!nokov::Decode(bytes, size, frame))
    {
        ReportError(u8"收到无效的动捕数据");
        return;
    }
    if (m_has_frame && m_recording &&
        (frame.header.session != m_latest_frame.header.session ||
         frame.header.sequence != m_latest_frame.header.sequence + 1))
        ReportError(u8"检测到动捕帧中断或会话变化，本轮记录已结束");
    if (m_has_frame && frame.header.session != m_latest_frame.header.session)
    {
        m_known_names.clear();
        m_selected_ids.clear();
    }
    for (const auto& marker : frame.markers)
    {
        const std::pair<std::string, std::string> name{marker.asset, marker.name};
        if (m_recording && (m_recorded_ids.empty() || m_recorded_ids.count(marker.id)))
        {
            if (!marker.asset[0] || !marker.name[0])
                ReportError(u8"点名称无法确认，本轮采集已结束，请检查后重新连接NOKOV");
            auto previous = m_recorded_names.find(marker.id);
            if (previous != m_recorded_names.end() && previous->second != name)
                ReportError(u8"点的名称对应关系发生变化，本轮采集已结束，请重新连接NOKOV");
            if (marker.asset[0] && marker.name[0])
            {
                for (const auto& other : m_recorded_names)
                    if (other.first != marker.id && other.second == name)
                        ReportError(u8"点的身份发生变化，请重新连接NOKOV后采集");
            }
            if (m_recording)
                m_recorded_names[marker.id] = name;
        }
        if (marker.asset[0] && marker.name[0])
        {
            m_known_names[marker.id] = name;
            if (m_record_all)
                m_selected_ids.insert(marker.id);
        }
    }
    if (m_has_frame && m_recording && frame.header.timestamp < m_latest_frame.header.timestamp)
        ReportError(u8"SDK时间戳发生回退，本轮采集已结束");
    m_latest_frame = std::move(frame);
    m_has_frame = true;
    ++m_received_frames;
    m_timing.last_frame = Clock::now();
    if (m_recording)
    {
        if (m_pending_frames.size() >= 2048)
            ReportError(u8"记录队列已满，本轮记录已结束");
        else
        {
            m_pending_frames.push_back(m_latest_frame);
            ++m_accepted_frames;
            m_write_cv.notify_one();
        }
    }
}

void UINokov::StartCapture(bool single)
{
    if (!m_connection_requested || !m_has_frame || Clock::now() - m_timing.last_frame > std::chrono::seconds(2))
    {
        ReportError(u8"没有近期帧数据，请先连接NOKOV");
        return;
    }
    if (!m_record_all && m_selected_ids.empty())
    {
        ReportError(u8"请至少选择一个标记点");
        return;
    }
    for (const auto& marker : m_latest_frame.markers)
        if (m_record_all || m_selected_ids.count(marker.id))
        {
            if (!marker.asset[0] || !marker.name[0])
            {
                ReportError(u8"存在未识别名称的点，请先确认名称再采集");
                return;
            }
        }
    for (int id : m_selected_ids)
        if (!m_record_all &&
            (!m_known_names.count(id) || m_known_names.at(id).first.empty() || m_known_names.at(id).second.empty()))
        {
            ReportError(u8"所选点缺少名称，请重新选择");
            return;
        }
    try
    {
        m_recording_path =
            (std::filesystem::u8path(m_save_directory) / ("NOKOV_" + std::to_string(nokov::NewLocalId()))).u8string();
        std::filesystem::create_directories(std::filesystem::u8path(m_recording_path));
        m_data_file.clear();
        m_data_file.open(std::filesystem::u8path(m_recording_path) / "data.csv", std::ios::out | std::ios::binary);
        if (!m_data_file)
            throw std::runtime_error(u8"无法打开记录文件");
        m_data_file << "\xef\xbb\xbf";
        m_data_file << std::setprecision(9) << CsvHeader;
        m_recorded_names.clear();
        m_recording_session = m_latest_frame.header.session;
        for (const auto& m : m_latest_frame.markers)
            m_recorded_names[m.id] = {m.asset, m.name};
        for (int id : m_selected_ids)
            if (m_known_names.count(id))
                m_recorded_names[id] = m_known_names[id];
        m_recorded_ids = m_record_all ? std::set<int>{} : m_selected_ids;
        m_timestamp_origin.reset();
        m_accepted_frames = m_written_frames = 0;
        m_incomplete = false;
        m_error.clear();
        m_single_capture = single;
        if (!single)
            UI_INFO(u8"[NOKOV] 开始连续采集，保存位置：%s", m_recording_path.c_str());
        m_recording = !single;
        m_finishing = single;
        if (single)
        {
            m_pending_frames.push_back(m_latest_frame);
            m_accepted_frames = 1;
        }
        m_write_cv.notify_one();
    }
    catch (const std::exception& e)
    {
        m_error = e.what();
        UI_ERROR(u8"[NOKOV] 无法开始保存：%s", m_error.c_str());
        m_data_file.close();
    }
}
void UINokov::WriteFrames()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    Clock::time_point nextFlush{};
    while (true)
    {
        m_write_cv.wait_for(lock, std::chrono::milliseconds(100),
                            [&] { return m_exiting || m_finishing || !m_pending_frames.empty(); });
        const auto rateNow = Clock::now();
        if (m_timing.rate_sample == Clock::time_point{})
        {
            m_timing.rate_sample = rateNow;
            m_timing.rate_frames = m_received_frames;
        }
        const auto rateElapsed = std::chrono::duration<double>(rateNow - m_timing.rate_sample).count();
        if (rateElapsed >= 1.0)
        {
            m_receive_hz = (m_received_frames - m_timing.rate_frames) / rateElapsed;
            m_timing.rate_sample = rateNow;
            m_timing.rate_frames = m_received_frames;
        }
        if (!m_connection_requested || !m_has_frame || rateNow - m_timing.last_frame > std::chrono::seconds(2))
            m_receive_hz = 0;
        size_t batch = 0;
        while (!m_pending_frames.empty() && batch++ < 128)
        {
            auto frame = std::move(m_pending_frames.front());
            m_pending_frames.pop_front();
            auto ids = m_recorded_ids;
            auto names = m_recorded_ids.empty() ? Names{} : m_recorded_names;
            // OnCPSMsg validates timestamp order before enqueueing; this FIFO preserves it.
            if (!m_timestamp_origin)
                m_timestamp_origin = frame.header.timestamp;
            const auto reference = *m_timestamp_origin;
            lock.unlock();
            WriteCsvRows(m_data_file, frame, ids, names, reference);
            lock.lock();
            if (!m_data_file)
            {
                ReportError(u8"数据写入失败，请检查保存目录和磁盘空间");
                m_pending_frames.clear();
                break;
            }
            ++m_written_frames;
        }
        const auto now = Clock::now();
        if (m_recording &&
            (now - m_timing.last_frame > std::chrono::seconds(2) || m_timing.last_status == Clock::time_point{} ||
             now - m_timing.last_status > std::chrono::seconds(3)))
            ReportError(u8"动捕数据流已中断，本轮记录已结束");
        if (m_data_file.is_open() && (now >= nextFlush || m_finishing))
        {
            lock.unlock();
            m_data_file.flush();
            lock.lock();
            if (!m_data_file)
            {
                ReportError(u8"数据文件刷新失败");
                m_pending_frames.clear();
            }
            nextFlush = Clock::now() + std::chrono::seconds(1);
        }
        if (m_finishing && m_pending_frames.empty())
            FinishCapture();
        if (m_exiting && m_pending_frames.empty())
            break;
    }
}
void UINokov::FinishCapture()
{
    if (m_data_file.is_open())
    {
        m_data_file.close();
        if (m_data_file.fail())
        {
            m_incomplete = true;
            m_error = u8"文件关闭失败";
        }
        if (m_accepted_frames != m_written_frames)
            m_incomplete = true;
        nlohmann::json mapping = nlohmann::json::array();
        for (const auto& entry : m_recorded_names)
        {
            if (!m_recorded_ids.empty() && !m_recorded_ids.count(entry.first))
                continue;
            const auto& name = entry.second;
            mapping.push_back({{"sdk_id", entry.first}, {"asset_name", name.first}, {"marker_name", name.second}});
        }
        std::ofstream mapFile(std::filesystem::u8path(m_recording_path) / "marker_map.json");
        mapFile << nlohmann::json({{"session", m_recording_session},
                                   {"coordinate_unit", "mm"},
                                   {"sdk_timestamp_unit", "ms"},
                                   {"time_unit", "s"},
                                   {"time_origin", "first_saved_frame_sdk_timestamp"},
                                   {"sdk_timestamp_origin_raw",
                                    m_timestamp_origin ? nlohmann::json(*m_timestamp_origin) : nlohmann::json(nullptr)},
                                   {"points", mapping}})
                       .dump(2);
        mapFile.close();
        if (!mapFile)
        {
            m_error = u8"名称对应表保存失败";
            m_incomplete = true;
        }
        if (m_incomplete)
        {
            if (m_error.empty())
                m_error = u8"本轮采集未完整写入";
            std::ofstream report(std::filesystem::u8path(m_recording_path) / "error.json");
            report << nlohmann::json({{"error", m_error},
                                      {"accepted_frames", m_accepted_frames},
                                      {"written_frames", m_written_frames}})
                          .dump(2);
            report.close();
            if (!report)
                m_error += u8"；错误说明也未能保存";
            UI_ERROR(u8"[NOKOV] %s未完整保存：%s。已写入 %llu 帧，保存位置：%s",
                     m_single_capture ? u8"单次采集" : u8"连续采集", m_error.c_str(), m_written_frames,
                     m_recording_path.c_str());
        }
        else
        {
            if (m_single_capture)
                UI_INFO(u8"[NOKOV] 单次保存成功，保存位置：%s", m_recording_path.c_str());
            else
                UI_INFO(u8"[NOKOV] 连续采集文件保存成功，共 %llu 帧，保存位置：%s", m_written_frames,
                        m_recording_path.c_str());
        }
    }
    m_finishing = false;
    if (m_disconnect_pending)
    {
        SendCommand(false);
        m_disconnect_pending = false;
    }
}

void UINokov::DrawPointSelection(int id, bool named)
{
    ImGui::PushID(id);
    bool chosen = m_record_all || m_selected_ids.count(id) != 0;
    ImGui::BeginDisabled(m_recording || m_finishing || !named);
    if (ImGui::Checkbox("##select", &chosen))
    {
        if (chosen)
            m_selected_ids.insert(id);
        else
            m_selected_ids.erase(id);
        m_record_all = !m_known_names.empty();
        for (const auto& point : m_known_names)
            if (!m_selected_ids.count(point.first))
            {
                m_record_all = false;
                break;
            }
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}

void UINokov::DrawPoints()
{
    ImGui::SeparatorText(u8"标记点选择与坐标预览");
    if (m_preview_frame.markers.empty())
        ImGui::TextWrapped(u8"%s", m_has_frame ? u8"已收到帧，本帧为空帧。"
                                               : u8"等待动捕帧数据。连接后将在下表显示点名称和 XYZ 坐标。");
    if (ImGui::BeginTable("MarkerPreview", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(u8"选择");
        ImGui::TableSetupColumn(u8"资产名称");
        ImGui::TableSetupColumn(u8"点名称");
        ImGui::TableSetupColumn(u8"X (mm)");
        ImGui::TableSetupColumn(u8"Y (mm)");
        ImGui::TableSetupColumn(u8"Z (mm)");
        ImGui::TableSetupColumn(u8"当前状态");
        ImGui::TableHeadersRow();
        for (auto& m : m_preview_frame.markers)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            DrawPointSelection(m.id, m.asset[0] && m.name[0]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m.asset[0] ? m.asset : u8"未识别名称");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m.name[0] ? m.name : "--");
            ImGui::TableNextColumn();
            ImGui::Text("%.6f", m.x);
            ImGui::TableNextColumn();
            ImGui::Text("%.6f", m.y);
            ImGui::TableNextColumn();
            ImGui::Text("%.6f", m.z);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(u8"存在");
        }
        for (const auto& entry : m_known_names)
        {
            const int id = entry.first;
            if (!m_selected_ids.count(id))
                continue;
            bool found = false;
            for (auto& m : m_preview_frame.markers)
                if (m.id == id)
                    found = true;
            if (!found)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                DrawPointSelection(id, true);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.second.first.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.second.second.c_str());
                for (int i = 0; i < 3; ++i)
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("--");
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(u8"缺失");
            }
        }
        ImGui::EndTable();
    }

    bool unresolved = false;
    for (const auto& m : m_preview_frame.markers)
        if (!m.asset[0] || !m.name[0])
            unresolved = true;
    if (unresolved)
        ImGui::TextWrapped(u8"部分点尚未匹配名称，请重新连接NOKOV。重合点不会被强行命名。");
}

void UINokov::Draw()
{
    if (!m_show)
        return;
    if (!ImGui::Begin(m_win_title, &m_show))
    {
        ImGui::End();
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto now = Clock::now();
    // Refresh displayed coordinates at 10 Hz; acquisition and writing remain per-frame.
    if (now - m_timing.preview_sample >= std::chrono::milliseconds(100))
    {
        m_preview_frame = m_latest_frame;
        m_timing.preview_sample = now;
    }
    auto api = g_app.GetCPSApi();
    const bool online = api && api->IsDeviceOnline(nokov::Device);
    ImGui::Text(u8"总线设备：%s | 动捕连接：%s | 数据流：%s", online ? u8"在线" : u8"离线",
                m_timing.last_status != Clock::time_point{} && now - m_timing.last_status < std::chrono::seconds(3)
                    ? (m_status.connected ? u8"已连接" : u8"未连接")
                    : u8"未知",
                m_has_frame && now - m_timing.last_frame < std::chrono::seconds(2) ? u8"接收中" : u8"无近期数据");
    if (NokovButton(ICON_FA_PLAY, u8" 连接NOKOV"))
        SendCommand(true);
    ImGui::SameLine();
    if (NokovButton(ICON_FA_STOP, u8" 断开NOKOV"))
    {
        if (m_recording || m_finishing)
        {
            m_disconnect_pending = true;
            StopCapture();
        }
        else
            SendCommand(false);
    }
    ImGui::Text(u8"最新帧号 %d | 当前点数 %zu | 本轮已写入 %llu 帧", m_preview_frame.header.frame,
                m_preview_frame.markers.size(), m_written_frames);
    ImGui::Text(u8"上位机接收：%.1f Hz", m_receive_hz);
    if (!m_recording && !m_finishing)
    {
        ImGui::InputText(u8"保存目录", m_save_directory, sizeof(m_save_directory));
        if (ImGui::Checkbox(u8"记录全部点", &m_record_all))
        {
            m_selected_ids.clear();
            if (m_record_all)
                for (const auto& entry : m_known_names)
                    m_selected_ids.insert(entry.first);
        }
    }
    DrawPoints();
    if (!m_recording && !m_finishing)
    {
        if (NokovButton(ICON_FA_ARROW_DOWN, u8" 单次保存"))
            StartCapture(true);
        ImGui::SameLine();
        if (NokovButton(ICON_FA_PLAY, u8" 连续采集"))
            StartCapture();
    }
    if (m_recording && NokovButton(ICON_FA_STOP, u8" 停止采集"))
        StopCapture();
    if (m_finishing)
        ImGui::TextUnformatted(u8"正在完成写入……");
    if (!m_recording_path.empty())
        ImGui::TextWrapped(u8"保存位置：%s", m_recording_path.c_str());
    if (!m_error.empty())
        ImGui::TextWrapped(u8"错误：%s", m_error.c_str());
    ImGui::End();
}
