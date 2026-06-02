#pragma once
#include <string>
#include "BufferedAsyncSerial.h"
#include "common_datadef.h"
#include "ModbusDef.h"
#include <mutex>

class SerialOtherReader
{
public:
    SerialOtherReader();
    ~SerialOtherReader();

    bool OpenSerialPort(const std::string& port_name, EMBBaudRate baud_rate);
    void CloseSerialPort();

    bool IsConnected();
    std::string GetLog();

    void SetLineTerminator(const std::string& terminator) { m_line_terminator = terminator; }
    const std::string& GetLineTerminator() const { return m_line_terminator; }
protected:
    bool CheckComConnection();
    void DestroyComConnection();

private:
    std::recursive_mutex m_serial_mutex; // Recursive mutex for thread safety
    BufferedAsyncSerial* m_serial_port; // Buffered async serial port for reading data
    EMBBaudRate m_baud_rate = E_115200;          // Default baud rate
    std::string m_port_name;
    std::string m_line_terminator = "\r\n"; // Line terminator for reading data
};
