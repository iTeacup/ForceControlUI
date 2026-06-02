#include "SerialOtherReader.h"
#include <spdlog/spdlog.h>

SerialOtherReader::SerialOtherReader()
{
}

SerialOtherReader::~SerialOtherReader()
{
    CloseSerialPort();
}

bool SerialOtherReader::OpenSerialPort(const std::string& port_name, EMBBaudRate baud_rate)
{
    CloseSerialPort(); // Ensure any previous connection is closed

    m_port_name = port_name;
    m_baud_rate = baud_rate;
    if (CheckComConnection())
    {
        return true;
    }
    else
    {
        SPDLOG_ERROR("Failed to open serial port: {}", port_name);
    }
    return false;
}

void SerialOtherReader::CloseSerialPort()
{
    DestroyComConnection();
}

bool SerialOtherReader::IsConnected()
{
    std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
    if (m_serial_port && m_serial_port->isOpen())
    {
        return true;
    }
    return false;
}


std::string SerialOtherReader::GetLog()
{
    std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
    if (!m_serial_port || !m_serial_port->isOpen())
    {
        SPDLOG_ERROR("Serial port is not open or does not exist.");
        return "";
    }
    return m_serial_port->readStringUntil(m_line_terminator);
}

bool SerialOtherReader::CheckComConnection()
{
    std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
    try
    {
        if (!m_serial_port)
        {
            m_serial_port = new BufferedAsyncSerial();
        }
        if (!m_serial_port->isOpen())
        {
            m_serial_port->open(m_port_name, GetBaudRate(m_baud_rate));
        }
        if (m_serial_port->errorStatus())
        {
            DestroyComConnection();
            return false;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR("open com: {} failed, {}", m_port_name, e.what());
    }
    return false;
}

void SerialOtherReader::DestroyComConnection()
{
    std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
    if (m_serial_port)
    {
        try
        {
            m_serial_port->close();
        }
        catch (const std::exception& e)
        {
            SPDLOG_ERROR("Failed to close serial port: {}", e.what());
        }
        delete m_serial_port;
        m_serial_port = nullptr;
    }
}
