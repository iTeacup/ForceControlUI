#include "UISyncCapture.h"
#include "UIBLEMonitor.h"
#include "UIApplication.h"
#include "UIUtils.h"
#include "UILog.h"
#include <CPSAPI/CPSAPI.h>
#include "IconsFontAwesome6.h"
#include "imgui/imgui.h"
#include <algorithm>

namespace
{
bool SyncButton(const char *icon, const char *label, const ImVec2& size = ImVec2(0, 0))
{
    return ImGui::Button((std::string(icon) + " " + label).c_str(), size);
}
} // namespace
UISyncCapture::UISyncCapture(UIMainWindowBase *main, const char *title, UIBLEMonitor *ble)
    : UIBaseWindow(main, title), m_ble(ble)
{
    if (m_ble)
        m_ble->SetCaptureObserver(
            [this](uint32_t id, uint64_t generation, const ST_BleAdcData &data, double host) {
                m_capture.Ble(id, generation, data.timestamp, data.channel_mv, host);
            },
            [this](uint32_t id, uint64_t generation, bool connected) {
                m_capture.BleState(id, generation, connected);
            });
}
UISyncCapture::~UISyncCapture()
{
    Shutdown();
}
void UISyncCapture::Shutdown()
{
    if (m_closed)
        return;
    m_closed = true;
    if (m_ble)
        m_ble->SetCaptureObserver();
    m_capture.Close();
}
void UISyncCapture::OnCPSMsg(uint32_t from, uint32_t type, const char *bytes, uint32_t size)
{
    const double host = sync_capture::HostSeconds();
    if (from != nokov::Device || !bytes)
        return;
    if (type == nokov::FrameMessage)
    {
        nokov::Frame frame;
        if (nokov::Decode(bytes, size, frame))
            m_capture.Nokov(std::move(frame), host);
        else
            m_capture.Fail(u8"收到无效 NOKOV 帧，请检查协议版本");
    }
    else if (type == nokov::StatusMessage)
    {
        nokov::Status status;
        if (size != sizeof(status))
        {
            m_capture.Fail(u8"NOKOV 状态协议不匹配");
            return;
        }
        memcpy(&status, bytes, size);
        m_capture.Status(status);
    }
}
void UISyncCapture::SendNokov(bool connect)
{
    auto api = g_app.GetCPSApi();
    if (!api || !api->IsDeviceOnline(nokov::Device))
    {
        UI_WARN(u8"[同步采集] 请先启动总线和 NOKOV 服务");
        return;
    }
    if (!connect)
        m_capture.Stop();
    nokov::Control command;
    command.connect = connect;
    if (api->SendAPPMsg(nokov::Device, nokov::ControlMessage, reinterpret_cast<const char *>(&command),
                        sizeof(command)))
        UI_ERROR(u8"[同步采集] NOKOV 命令发送失败");
    else
        UI_INFO(u8"[同步采集] 已发送 NOKOV %s请求", connect ? u8"连接" : u8"断开");
}
void UISyncCapture::LogResult(bool success)
{
    m_view = m_capture.Snapshot();
    if (success)
        UI_INFO(u8"[同步采集] %s",
                m_view.phase == sync_capture::Capture::Phase::Recording ? u8"开始联合采集" : u8"正在生成对齐数据集");
    else
        UI_WARN(u8"[同步采集] %s", m_view.error.c_str());
}
void UISyncCapture::DrawPoints(float tableHeight)
{
    const bool busy = m_view.phase != sync_capture::Capture::Phase::Idle;
    ImGui::SeparatorText(u8"实时数据预览");
    if (ImGui::BeginTable("SyncVoltagePreview", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        for (const char* label : {"CH1 (mV)", "CH2 (mV)", "CH3 (mV)", "CH4 (mV)"})
            ImGui::TableSetupColumn(label);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        for (float value : m_view.channels)
        {
            ImGui::TableNextColumn();
            if (m_view.bleReady) ImGui::Text("%.3f", value); else ImGui::TextDisabled("--");
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::SeparatorText(u8"标记点 · 坐标预览");
    ImGui::BeginDisabled(busy);
    if (ImGui::Checkbox(u8"记录全部点", &m_all))
    {
        m_selected.clear();
        if (m_all)
            for (const auto &p : m_view.names)
                m_selected.insert(p.first);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled(u8"已选 %zu / %zu 点", m_selected.size(), m_view.names.size());
    if (m_view.preview.markers.empty())
        ImGui::TextUnformatted(m_view.nokovReady ? u8"已收到帧，本帧为空帧。" : u8"等待动捕帧数据。");
    if (tableHeight <= 0)
        tableHeight = std::max(ImGui::GetTextLineHeightWithSpacing() * 5,
                              ImGui::GetWindowHeight() - ImGui::GetCursorPosY() -
                              ImGui::GetStyle().WindowPadding.y - ImGui::GetStyle().ItemSpacing.y);
    if (!ImGui::BeginTable("SyncPoints", 7,
                           ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                           ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                           ImVec2(0, tableHeight), std::max(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 34)))
        return;
    ImGui::TableSetupScrollFreeze(1, 1);
    ImGui::TableSetupColumn(u8"选择", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 3);
    ImGui::TableSetupColumn(u8"资产名称", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn(u8"点名称", ImGuiTableColumnFlags_WidthStretch);
    for (const char* label : {"X (mm)", "Y (mm)", "Z (mm)"})
        ImGui::TableSetupColumn(label, ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.5f);
    ImGui::TableSetupColumn(u8"状态", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 3);
    ImGui::TableHeadersRow();
    for (const auto &entry : m_view.names)
    {
        const nokov::Marker *marker = nullptr;
        for (const auto &p : m_view.preview.markers)
            if (p.id == entry.first)
            {
                marker = &p;
                break;
            }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(entry.first);
        bool chosen = m_selected.count(entry.first) != 0;
        ImGui::BeginDisabled(busy);
        if (ImGui::Checkbox("##point", &chosen))
        {
            if (chosen)
                m_selected.insert(entry.first);
            else
                m_selected.erase(entry.first);
            m_all = !m_view.names.empty() && m_selected.size() == m_view.names.size();
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(entry.second.first.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(entry.second.second.c_str());
        for (int axis = 0; axis < 3; ++axis)
        {
            ImGui::TableNextColumn();
            if (marker && m_view.nokovReady)
                ImGui::Text("%.3f", axis == 0 ? marker->x : axis == 1 ? marker->y : marker->z);
            else
                ImGui::TextUnformatted("--");
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(marker && m_view.nokovReady ? u8"存在" : u8"缺失");
    }
    ImGui::EndTable();
}
void UISyncCapture::DrawDevices()
{
    const bool busy = m_view.phase != sync_capture::Capture::Phase::Idle;
    const auto devices = m_ble ? m_ble->Devices() : std::vector<UIBLEMonitor::DeviceInfo>{};
    const UIBLEMonitor::DeviceInfo* chosen = nullptr;
    for (const auto& p : devices)
        if (p.id == m_bleId) chosen = &p;
    ImGui::SeparatorText(u8"BLE · 连接");
    ImGui::PushID("BLE");
    ImGui::BeginDisabled(busy);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##SyncBLEDevice", chosen ? chosen->name.c_str() : u8"请选择设备"))
    {
        for (const auto& p : devices)
            if (ImGui::Selectable((p.name + "##" + std::to_string(p.id)).c_str(), p.id == m_bleId))
            {
                m_bleId = p.id;
                m_capture.SelectBle(p.id);
                m_capture.BleState(p.id, p.generation, p.connected);
                chosen = &p;
            }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!chosen || chosen->connected || chosen->busy || busy);
    if (SyncButton(ICON_FA_PLAY, u8"连接 BLE")) m_ble->ConnectDevice(m_bleId, true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!chosen || (!chosen->connected && !chosen->busy));
    if (SyncButton(ICON_FA_STOP, u8"断开")) m_ble->ConnectDevice(m_bleId, false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(u8"设置") && m_ble)
    {
        m_ble->Show();
        ImGui::SetWindowFocus(m_ble->GetWinTitle());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"广播名和 UUID 在蓝牙监控中设置，连接共用。");
    ImGui::Text(u8"%s · %.1f Hz", m_view.bleReady ? u8"数据就绪" : u8"等待有效数据", m_view.bleHz);
    ImGui::PopID();
    ImGui::Spacing();
    ImGui::SeparatorText(u8"NOKOV · 连接");
    ImGui::BeginDisabled(busy);
    if (SyncButton(ICON_FA_PLAY, u8"连接 NOKOV")) SendNokov(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (SyncButton(ICON_FA_STOP, u8"断开")) SendNokov(false);
    ImGui::Text(u8"%s · %.1f Hz", m_view.nokovReady ? u8"数据就绪" : u8"等待有效数据", m_view.nokovHz);
}

void UISyncCapture::DrawOutputSettings()
{
    const bool busy = m_view.phase != sync_capture::Capture::Phase::Idle;
    ImGui::SeparatorText(u8"数据集输出");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"初步软件对齐，精度待验证。\n插值只在相邻有效数据之间进行；缺失点和过大间隔对应的行不输出。\n降低输出 Hz 不自动抗混叠滤波。");
    ImGui::BeginDisabled(busy);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##SyncDirectory", u8"保存目录", m_directory, sizeof(m_directory));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"采集保存目录：\n%s", m_directory);
    if (ImGui::BeginTable("SyncOutputSettings", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        auto field = [](const char* label) {
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label);
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
        };
        field(u8"输出基准");
        int basis = static_cast<int>(m_options.basis);
        if (ImGui::Combo("##SyncBasis", &basis, u8"BLE 采样时刻\0NOKOV 帧时刻\0固定时间间隔\0"))
            m_options.basis = static_cast<sync_capture::Basis>(basis);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(u8"选择对齐数据集使用的时间点。初步软件对齐，精度待验证。\n详细说明可悬停在“数据集输出”标题查看。");
        if (m_options.basis == sync_capture::Basis::Fixed)
        {
            field(u8"固定输出 Hz");
            ImGui::InputDouble("##SyncFrequency", &m_options.frequency, 0, 0, "%.1f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"固定时间间隔的输出频率。降低输出 Hz 不自动抗混叠滤波。");
        }
        double gap = m_options.maxGap * 1000, offset = m_options.bleOffset * 1000;
        field(u8"插值间隔 ms");
        if (ImGui::InputDouble("##SyncGap", &gap, 0, 0, "%.1f")) m_options.maxGap = gap / 1000;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(u8"允许插值的最大相邻有效数据间隔。\n缺失点和超过该间隔的行不输出。");
        field(u8"BLE 修正 ms");
        if (ImGui::InputDouble("##SyncOffset", &offset, 0, 0, "%.1f")) m_options.bleOffset = offset / 1000;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(u8"BLE 时间修正为正时，该路数据在公共时间轴上后移。");
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText(u8"历史采集重新导出");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"使用原始采集目录保存的标记点和当前输出参数，保留原始记录并替换数据集。");
    ImGui::BeginDisabled(busy);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##SyncHistoryDirectory", u8"原始采集目录", m_historyDirectory, sizeof(m_historyDirectory));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"原始采集目录：\n%s\n使用该目录保存的标记点和当前输出参数，保留原始记录并替换数据集。", m_historyDirectory);
    ImGui::BeginDisabled(!m_historyDirectory[0]);
    if (SyncButton(ICON_FA_ARROW_DOWN, u8"从目录生成数据集", ImVec2(-1, 0)))
        LogResult(m_capture.Reexport(m_options, std::filesystem::u8path(m_historyDirectory)));
    ImGui::EndDisabled();
    ImGui::EndDisabled();
}

void UISyncCapture::DrawCaptureControl()
{
    const bool busy = m_view.phase != sync_capture::Capture::Phase::Idle;
    ImGui::SeparatorText(u8"联合采集控制");
    ImGui::BeginDisabled(busy || !m_view.bleReady || !m_view.nokovReady || m_selected.empty());
    if (SyncButton(ICON_FA_PLAY, u8"开始联合采集"))
        LogResult(m_capture.Start(std::filesystem::u8path(m_directory), m_selected, m_options));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_view.phase != sync_capture::Capture::Phase::Recording);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.24f, 1));
    if (SyncButton(ICON_FA_STOP, u8"停止采集"))
    {
        m_capture.Stop();
        UI_INFO(u8"[同步采集] 正在完成原始记录并生成数据集");
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(busy || m_view.path.empty());
    if (SyncButton(ICON_FA_ARROW_DOWN, u8"重新生成数据集")) LogResult(m_capture.Reexport(m_options));
    ImGui::EndDisabled();
    ImGui::Text(u8"已保存：BLE %llu 组 | NOKOV %llu 帧 | 对齐表 %llu 行", m_view.bleWritten, m_view.nokovWritten, m_view.rows);
    if (m_view.phase == sync_capture::Capture::Phase::Finishing)
        ImGui::TextUnformatted(u8"正在完成写入与对齐，请稍候……");
    else if (!m_view.message.empty()) ImGui::TextWrapped("%s", m_view.message.c_str());
    if (!m_view.path.empty()) ImGui::TextWrapped(u8"保存位置：%s", m_view.path.c_str());
    if (!m_view.error.empty()) ImGui::TextWrapped(u8"错误：%s", m_view.error.c_str());
}

void UISyncCapture::Draw()
{
    if (m_closed)
        return;
    const double now = sync_capture::HostSeconds();
    if (now - m_lastRefresh >= 0.1)
    {
        m_view = m_capture.Snapshot();
        m_lastRefresh = now;
        if (m_view.completed != m_seenCompletion)
        {
            m_seenCompletion = m_view.completed;
            if (m_view.error.empty())
                UI_INFO(u8"[同步采集] %s，共 %llu 行，位置：%s", m_view.message.c_str(), m_view.rows,
                        m_view.path.c_str());
            else
                UI_ERROR(u8"[同步采集] %s；位置：%s", m_view.error.c_str(), m_view.path.c_str());
        }
        if (m_view.phase == sync_capture::Capture::Phase::Idle)
        {
            if (m_previewSession != m_view.preview.header.session)
            {
                m_selected.clear();
                m_previewSession = m_view.preview.header.session;
            }
            if (m_all)
                for (const auto &p : m_view.names)
                    m_selected.insert(p.first);
            for (auto it = m_selected.begin(); it != m_selected.end();)
                if (!m_view.names.count(*it))
                    it = m_selected.erase(it);
                else
                    ++it;
        }
    }
    if (!m_show)
        return;
    if (!ImGui::Begin(m_win_title, &m_show))
    {
        ImGui::End();
        return;
    }
    const bool compact = ImGui::GetContentRegionAvail().y < ImGui::GetFontSize() * 28;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, compact ? 2.0f : 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, compact ? 1.0f : 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(5, compact ? 1.0f : 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 5.5f;
    const float height = std::max(100.0f, ImGui::GetContentRegionAvail().y - footer);
    const bool wide = ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 48;
    if (wide && ImGui::BeginTable("SyncPanels", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Settings", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 21);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::BeginChild("SyncSettings", ImVec2(0, height), true);
        DrawDevices();
        DrawOutputSettings();
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("SyncPreview", ImVec2(0, height), true);
        DrawPoints();
        ImGui::EndChild();
        ImGui::EndTable();
    }
    else if (!wide)
    {
        ImGui::BeginChild("SyncStacked", ImVec2(0, height), true);
        if (ImGui::BeginTabBar("SyncPages"))
        {
            if (ImGui::BeginTabItem(u8"连接与输出"))
            {
                DrawDevices(); DrawOutputSettings(); ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"实时预览"))
            {
                DrawPoints(); ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
    }
    ImGui::BeginChild("SyncControl", ImVec2(0, 0), true);
    DrawCaptureControl();
    ImGui::EndChild();
    ImGui::PopStyleVar(4);
    ImGui::End();
}
