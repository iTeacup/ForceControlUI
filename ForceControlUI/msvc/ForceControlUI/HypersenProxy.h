#pragma once
#include "BufferedAsyncSerial.h"
#include "HypersenDef.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include "ModbusDef.h"

class HypersenProxy
{
public:
    HypersenProxy();
    ~HypersenProxy();

    bool Connect(const std::string &com, EMBBaudRate baudrate);
    void Disconnect();

    bool IsSensorOK();
    bool IsReading();

    void ReadSensorDevID();
    void ReadVersion();

    void StartMeasure();
    void StopMeasure();
    void ResetZero();

    ST_HypersenSensorData GetCurrentData();
    ST_HypersenSensorStatus GetSensorStatus();
    ST_HypersenSensorInfo GetSensorInfo();

protected:
    void ReadThreadFunc();

    bool CheckComConnection();
    void DestroyComConnection();

protected:
    std::string m_com_name;
    EMBBaudRate m_baud_rate = E_9600;

    BufferedAsyncSerial *m_com = nullptr;
    std::atomic_bool m_is_sensor_ok = false;

    bool m_exit_flag = false;
    std::thread m_thread_handle;

    std::mutex m_cur_data_lock;
    ST_HypersenSensorData m_cur_data = {0};

    std::mutex m_sensor_status_lock;
    ST_HypersenSensorStatus m_sensor_status = {0};

    std::mutex m_sensor_info_lock;
    ST_HypersenSensorInfo m_sensor_info = {0};
};
