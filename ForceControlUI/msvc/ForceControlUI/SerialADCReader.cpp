#include "SerialADCReader.h"
#include <spdlog/spdlog.h>

// ADC数据二进制串口消息头
const uint8_t ADC_UART_MSG_HEADER[2] = {0xAA, 0xBB};
// ADC数据消息长度（不包括头和长度字段）
#define ADC_UART_MSG_DATA_LEN sizeof(adc_info_t)
// ADC数据二进制串口消息尾
const uint8_t ADC_UART_MSG_FOOTER[2] = {0xBB, 0xAA};
// ADC数据消息总长度（消息头+数据+校验和+消息尾）
#define ADC_UART_MSG_TOTAL_LEN (2 + ADC_UART_MSG_DATA_LEN + 2 + 2)

// 计算CRC16校验和
static uint16_t crc16_compute(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}
//////////////////////////////////////////////////////////////////////////////////
SerialADCReader::SerialADCReader()
{
}

SerialADCReader::~SerialADCReader()
{
    CloseSerialPort();
}

bool SerialADCReader::OpenSerialPort(const std::string &port_name, EMBBaudRate baud_rate)
{
    CloseSerialPort(); // Ensure any previous connection is closed

    m_port_name = port_name;
    m_baud_rate = baud_rate;
    if (!CheckComConnection())
    {
        SPDLOG_ERROR("Failed to open serial port: {}", port_name);
    }
    // start read thread
    m_stop_thread = false;
    m_read_thread = std::thread(&SerialADCReader::ReadThreadFunc, this);
    return true;
}

void SerialADCReader::CloseSerialPort()
{
    m_stop_thread = true;
    if (m_read_thread.joinable())
    {
        m_read_thread.join();
    }

    DestroyComConnection();
}

bool SerialADCReader::IsConnected()
{
    std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
    if (m_serial_port && m_serial_port->isOpen())
    {
        return true;
    }
    return false;
}

void SerialADCReader::RegisterADCDataCallback(ADCDataCallback callback)
{
    m_adc_data_callbacks.push_back(callback);
}

bool SerialADCReader::CheckComConnection()
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
    catch (const std::exception &e)
    {
        SPDLOG_ERROR("open com: {} failed, {}", m_port_name, e.what());
    }
    return false;
}

void SerialADCReader::DestroyComConnection()
{
    std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
    if (m_serial_port)
    {
        try
        {
            m_serial_port->close();
        }
        catch (const std::exception &e)
        {
            SPDLOG_ERROR("Failed to close serial port: {}", e.what());
        }
        delete m_serial_port;
        m_serial_port = nullptr;
    }
}

void SerialADCReader::ReadThreadFunc()
{
    std::vector<uint8_t> read_buffer;
    while (!m_stop_thread)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_serial_mutex); // Ensure thread safety
            if (m_serial_port && m_serial_port->isOpen())
            {
                std::vector<char> data = m_serial_port->read();
                if (!data.empty())
                {
                    read_buffer.reserve(read_buffer.size() + data.size());
                    for (char ch : data)
                    {
                        // 显式转换，避免负值
                        read_buffer.push_back(static_cast<uint8_t>(static_cast<unsigned char>(ch)));
                    }
                }
            }
        }
        // Sleep if not enough data
        if (read_buffer.size() < ADC_UART_MSG_TOTAL_LEN)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Process the read buffer for complete ADC messages
        while (read_buffer.size() >= ADC_UART_MSG_TOTAL_LEN && !m_stop_thread)
        {
            // Look for message header
            auto it = std::search(read_buffer.begin(), read_buffer.end(),
                                  ADC_UART_MSG_HEADER, ADC_UART_MSG_HEADER + 2);
            if (it == read_buffer.end())
            {
                // No header found, clear buffer
                read_buffer.clear();
                break;
            }
            if (it != read_buffer.begin())
            {
                // Remove data before header
                read_buffer.erase(read_buffer.begin(), it);
            }
            if (read_buffer.size() < ADC_UART_MSG_TOTAL_LEN)
            {
                // Not enough data for a full message
                break;
            }

            // offsets
            const size_t payload_offset = 2;
            const size_t crc_offset = payload_offset + ADC_UART_MSG_DATA_LEN;
            const size_t footer_offset = crc_offset + 2;

            // Check message footer
            if (read_buffer[footer_offset] != ADC_UART_MSG_FOOTER[0] ||
                read_buffer[footer_offset + 1] != ADC_UART_MSG_FOOTER[1])
            {
                // Invalid footer, remove header and continue
                read_buffer.erase(read_buffer.begin(), read_buffer.begin() + 2);
                continue;
            }

            // Verify CRC16 (假定小端序: 低字节在前)
            uint16_t received_crc = static_cast<uint16_t>(read_buffer[crc_offset]) |
                                    (static_cast<uint16_t>(read_buffer[crc_offset + 1]) << 8);
            uint16_t computed_crc = crc16_compute(&read_buffer[payload_offset],
                                                  static_cast<uint16_t>(ADC_UART_MSG_DATA_LEN));
            if (received_crc != computed_crc)
            {
                // Invalid CRC, remove header and continue
                read_buffer.erase(read_buffer.begin(), read_buffer.begin() + 2);
                continue;
            }

            // Valid message, extract ADC data
            adc_info_t adc_data;
            memcpy(&adc_data, &read_buffer[payload_offset], sizeof(adc_info_t));

            // Invoke callback with ADC data
            for (const auto &callback : m_adc_data_callbacks)
            {
                if (callback)
                {
                    callback(&adc_data);
                }
            }

            // Remove the processed message
            read_buffer.erase(read_buffer.begin(), read_buffer.begin() + ADC_UART_MSG_TOTAL_LEN);
        }
    }
}
