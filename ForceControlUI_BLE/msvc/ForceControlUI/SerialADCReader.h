#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "BufferedAsyncSerial.h"
#include "common_datadef.h"
#include "ModbusDef.h"

typedef std::function<void(const adc_info_t *adc_data)> ADCDataCallback;

class SerialADCReader
{
public:
    SerialADCReader();
    ~SerialADCReader();

    bool OpenSerialPort(const std::string &port_name, EMBBaudRate baud_rate);
    void CloseSerialPort();

    bool IsConnected();

    void RegisterADCDataCallback(ADCDataCallback callback);

protected:
    bool CheckComConnection();
    void DestroyComConnection();

    void ReadThreadFunc();

private:
    std::recursive_mutex m_serial_mutex; // Recursive mutex for thread safety
    BufferedAsyncSerial *m_serial_port;  // Buffered async serial port for reading data
    EMBBaudRate m_baud_rate = E_115200;  // Default baud rate
    std::string m_port_name;

    std::vector<ADCDataCallback> m_adc_data_callbacks;

    std::thread m_read_thread;
    bool m_stop_thread = false;
};
