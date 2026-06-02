#pragma once
#include "UIBaseWindow.h"
#include <mutex>
#include <vector>
#include <string>
#include "HypersenProxy.h"
#include "UIPlotDef.h"
#include "ModbusDef.h"

class UIHypersenMonitor : public UIBaseWindow
{
public:
    UIHypersenMonitor(UIMainWindowBase *main_win, const char *title);
    ~UIHypersenMonitor();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char *GetShowShortCut() { return "Ctrl+3"; }

    HypersenProxy *GetHypersenProxy() { return &m_hypersen_proxy; }
protected:
    HypersenProxy m_hypersen_proxy;
    EMBBaudRate m_baud_rate = E_9600;

    std::vector<std::string> m_vec_coms;
    size_t m_com_index = 6;

    bool m_connect_failed_warning = false;
    std::string m_warning_msg = "";

    std::mutex m_vec_hist_data_lock;
    std::vector<ScrollingBuffer> m_vec_hist_data;
};
