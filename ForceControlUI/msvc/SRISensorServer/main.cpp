#include <iostream>
#include <string>
#include "SRICfgParser.h"
#include "SRICPSHandler.h"
#include "sriRDSerial/sriCommDefine.h"
#include "sriRDSerial/sriCommManager.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <atomic>

// 全局运行标志
static std::atomic_bool g_running{true};

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_running = false; // 通知主循环退出
        return TRUE;
    default:
        return FALSE;
    }
}

int main()
{
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE))
    {
        std::cerr << "Failed to set console control handler." << std::endl;
        return -1;
    }
    // Load configuration
    if (!g_cfg->LoadCfg())
    {
        return -1;
    }
    CCPSAPI *cps_api = CCPSAPI::CreateAPI();
    if (!cps_api)
    {
        std::cerr << "Create CPS API instance failed." << std::endl;
        return -1;
    }
    CSRICommManager commManager;
    if (!commManager.Init(g_cfg->m_sri_cfg.com_port))
    {
        std::cerr << "Force sensor Init failed." << std::endl;
        cps_api->Release();
        return -1;
    }

    CSRICPSHandler handler(cps_api, &commManager);
    int ret = cps_api->Init(E_CPS_TYPE_DEVICE, g_cfg->m_dev_id, 0,
                            g_cfg->m_cpscfg.bus.ip, g_cfg->m_cpscfg.bus.port,
                            g_cfg->m_cpscfg.log.ip, g_cfg->m_cpscfg.log.port,
                            &handler);
    if (ret != 0)
    {
        std::cerr << "CPS API Init failed, ret=" << ret << std::endl;
        cps_api->Release();
        return -1;
    }
    std::cout << "CPS API Init success." << std::endl;

    if (!commManager.Run(g_cfg->m_sri_cfg.baud_rate, g_cfg->m_sri_cfg.sample_rate))
    {
       std::cerr << "Force sensor Run failed." << std::endl;
       cps_api->Release();
       return -1;
    }

    ST_SRISensorData sensor_data = {0};
    while (g_running)
    {
        commManager.GetSensorData(sensor_data.fx_main, sensor_data.fy_main, sensor_data.fz_main,
                                   sensor_data.mx_main, sensor_data.my_main, sensor_data.mz_main);
        handler.PushSensorData(&sensor_data);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / g_cfg->m_push_freq_hz));
    }
    commManager.Stop();
    cps_api->Release();
    std::cout << "Application Exit." << std::endl;
    return 0;
}
