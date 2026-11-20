#pragma once
#include "UIBaseWindow.h"
#include "SyncCapture.h"

class UIBLEMonitor;
class UISyncCapture : public UIBaseWindow
{
  public:
    UISyncCapture(UIMainWindowBase *, const char *, UIBLEMonitor *);
    ~UISyncCapture() override;
    EUIMenuCategory GetWinMenuCategory() override
    {
        return EUIMenuCategory::E_UI_CAT_APP;
    }
    void Draw() override;
    void OnCPSMsg(uint32_t, uint32_t, const char *, uint32_t) override;
    void Shutdown();

  private:
    void SendNokov(bool connect);
    void DrawPoints(float tableHeight = 0);
    void DrawDevices();
    void DrawOutputSettings();
    void DrawCaptureControl();
    void LogResult(bool success);
    UIBLEMonitor *m_ble;
    sync_capture::Capture m_capture;
    sync_capture::Capture::View m_view;
    sync_capture::Options m_options;
    std::set<int> m_selected;
    bool m_all = true, m_closed = false;
    uint32_t m_bleId = 1;
    uint64_t m_previewSession = 0;
    uint64_t m_seenCompletion = 0;
    double m_lastRefresh = 0;
    char m_directory[512] = "data/sync";
    char m_historyDirectory[512] = {};
};
