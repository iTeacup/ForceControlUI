#include "HypersenProxy.h"
#include "Logger.h"

enum class SENSOR_CMD
{
    CMD_NONE = 0x0,
    CMD_READ_DEV_ID,
    CMD_CONTINUOUS_READ,
    CMD_STOP_READ,
    CMD_SINGLE_READ,
    CMD_SET_FREQ,
    CMD_SET_BAUDRATE,
    CMD_RESTORE_USER_SETTINGS,
    CMD_RESTORE_FACTORY_SETTINGS,
    CMD_STORE_USER_SETTINGS,
    CMD_GET_VERSION,
    CMD_RESET_ZERO,
    CMD_RESET_DEVICE_ADDR = 0xD,
    CMD_SET_DEVICE_ADDR = 0xE,
    CMD_SET_LOW_FILTER_PARAM = 0x18,
    CMD_GET_OVERFLOW_NUM = 0xD4,
    CMD_GET_OVERFLOW_VALUE = 0xD8
};

HypersenProxy::HypersenProxy()
{
}

HypersenProxy::~HypersenProxy()
{
    Disconnect();
}

bool HypersenProxy::Connect(const std::string &com, EMBBaudRate baudrate)
{
    Disconnect();

    m_com_name = com;
    m_baud_rate = baudrate;

    m_exit_flag = false;
    m_thread_handle = std::thread(&HypersenProxy::ReadThreadFunc, this);
    return true;
}

void HypersenProxy::Disconnect()
{
    m_exit_flag = true;
    if (m_thread_handle.joinable())
    {
        m_thread_handle.join();
    }
    DestroyComConnection();
}

bool HypersenProxy::IsSensorOK()
{
    return m_is_sensor_ok.load();
}

bool HypersenProxy::IsReading()
{
    return m_sensor_status.status == 1;
}

void HypersenProxy::ReadSensorDevID()
{
    if (!m_com)
        return;
    uint8_t cmd[] = {0xF6, 0x6F, 0x03, 0x00, 0x00, 0x01, 0xBD, 0xDC, 0x6F, 0xF6};
    m_com->write((const char *)cmd, sizeof(cmd));
}

void HypersenProxy::ReadVersion()
{
    if (!m_com)
        return;
    uint8_t cmd[] = {0xF6, 0x6F, 0x03, 0x00, 0x00, 0x0A, 0xD6, 0x6D, 0x6F, 0xF6};
    m_com->write((const char *)cmd, sizeof(cmd));
}

void HypersenProxy::StartMeasure()
{
    /*传感器在上电后，需要预热一段时间以稳定输出，建议以最高的测量频率工作 10~20 分钟，使内部器件
    受热平衡后发送零点复位命令，复位完成后再开始使用。
    */
    if (!m_com)
        return;
    uint8_t cmd[] = {0xF6, 0x6F, 0x03, 0x00, 0x00, 0x02, 0xDE, 0xEC, 0x6F, 0xF6};
    m_com->write((const char *)cmd, sizeof(cmd));
}

void HypersenProxy::StopMeasure()
{
    /*
    注意：
    如果传感器当前处于连续测量模式，在发送停止测量命令前，需要先发送 50 个字节的 0x00 占用 RS-485
    总线，传感器收到这些字节后会暂时释放 RS-485 总线。主机在发送完 50 个字节的 0x00 数据后延时约 200ms
    后发送停止测量命令即可正确停止传感器测量。传感器在接收到停止测量命令后，会返回一个应答数据包，表
    示已经正确停止了测量。如果主机在发送完 50 个字节的 0x00 数据后 250ms 内还未发送停止测量命令，则传
    感器会自动重新启动连续测量。如果传感器正处于连续测量模式下，在发送其他任何命令前，必须先停止连续
    测量，传感器在正确停止测量后才能正确接收其他的命令。
    */
    if (!m_com)
        return;
    char pre_cmd[50] = {0};
    m_com->write(pre_cmd, sizeof(pre_cmd));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    uint8_t cmd[] = {0xF6, 0x6F, 0x03, 0x00, 0x00, 0x03, 0xFF, 0xFC, 0x6F, 0xF6};
    m_com->write((const char *)cmd, sizeof(cmd));

    {
        std::lock_guard<std::mutex> status_lock(m_sensor_status_lock);
        m_sensor_status.status = 0; // stopped
    }
}

void HypersenProxy::ResetZero()
{
    if (!m_com)
        return;
    uint8_t cmd[] = {0xF6, 0x6F, 0x03, 0x00, 0x00, 0x0B, 0xF7, 0x7D, 0x6F, 0xF6};
    m_com->write((const char *)cmd, sizeof(cmd));
}

ST_HypersenSensorData HypersenProxy::GetCurrentData()
{
    std::lock_guard<std::mutex> lock(m_cur_data_lock);
    return m_cur_data;
}

ST_HypersenSensorStatus HypersenProxy::GetSensorStatus()
{
    std::lock_guard<std::mutex> lock(m_sensor_status_lock);
    return m_sensor_status;
}

ST_HypersenSensorInfo HypersenProxy::GetSensorInfo()
{
    std::lock_guard<std::mutex> lock(m_sensor_info_lock);
    return m_sensor_info;
}

void HypersenProxy::ReadThreadFunc()
{
    // constexpr size_t SENSOR_HEADER_LEN = 5;
    constexpr size_t SENSOR_DATA_MAX_LEN = 64;
    while (!m_exit_flag)
    {
        if (!CheckComConnection())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        auto wait_bytes = [&](size_t bytes)
        {
            while (!m_exit_flag && m_com->bytesRead() < bytes)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        wait_bytes(1);
        // 帧头两个字节为 0xF6, 0x6F
        // 帧尾前两个字节为CRC，后两个字节为0x6F, 0xF6
        // 第三个字节为数据长度，第四个字节为设备地址，第五个字节为保留字节，第6个字节为命令
        // 其余字节为数据
        // read header
        char temp_buf[1];
        if (m_com->read(temp_buf, 1) != 1 ||
            temp_buf[0] != (char)0xF6)
        {
            LOG_ERROR("Invalid frame header");
            continue;
        }
        wait_bytes(1);
        if (m_com->read(temp_buf, 1) != 1 ||
            temp_buf[0] != (char)0x6F)
        {
            LOG_ERROR("Invalid frame header");
            continue;
        }
        // read the rest of the header
        wait_bytes(4);
        char head_buf[4] = {0};
        m_com->read(head_buf, 4);

        uint8_t data_length = head_buf[0];
        uint8_t device_addr = head_buf[1];
        uint8_t reserved = head_buf[2];
        SENSOR_CMD cmd = static_cast<SENSOR_CMD>(head_buf[3]);

        size_t body_len = data_length - 3;
        if (body_len > SENSOR_DATA_MAX_LEN)
        {
            LOG_ERROR("Invalid body length: %zu", body_len);
            continue;
        }
        // read body
        wait_bytes(body_len);
        char body_buf[SENSOR_DATA_MAX_LEN] = {0};
        m_com->read(body_buf, body_len);

        // read CRC
        wait_bytes(2);
        char crc_buf[2] = {0};
        m_com->read(crc_buf, 2);
        // check CRC
        // read tail
        wait_bytes(2);
        char tail_buf[2] = {0};
        m_com->read(tail_buf, 2);
        if (tail_buf[0] != (char)0x6F || tail_buf[1] != (char)0xF6)
        {
            LOG_ERROR("Invalid frame tail");
            continue;
        }

        switch (cmd)
        {
        case SENSOR_CMD::CMD_READ_DEV_ID:
        {
            std::lock_guard<std::mutex> lock(m_sensor_info_lock);
            m_sensor_info.dev_id = (uint16_t(body_buf[1]) << 1) + body_buf[0];
        }
        break;
        case SENSOR_CMD::CMD_CONTINUOUS_READ:
        {
            std::lock_guard<std::mutex> lock(m_cur_data_lock);
            m_cur_data.code = reserved;
            for (unsigned int i = 0; i < HYPERSEN_SENSOR_DOF; i++)
            {
                // 将传感器数据转换为整数，四个字节按照小端格式存储
                int32_t v = (static_cast<uint8_t>(body_buf[i * 4 + 0]) |
                             (static_cast<uint8_t>(body_buf[i * 4 + 1]) << 8) |
                             (static_cast<uint8_t>(body_buf[i * 4 + 2]) << 16) |
                             (static_cast<uint8_t>(body_buf[i * 4 + 3]) << 24));
                m_cur_data.data[i] = v / 1000.0f;
            }
            if (m_cur_data.code == 0)
            {
                std::lock_guard<std::mutex> status_lock(m_sensor_status_lock);
                m_sensor_status.status = 1; // reading
            }
            else
            {
                std::lock_guard<std::mutex> status_lock(m_sensor_status_lock);
                m_sensor_status.status = 2; // error
            }
        }
        break;
        case SENSOR_CMD::CMD_STOP_READ:
        {
            char stop_flag = body_buf[0];
            if (stop_flag == 0x01)
            {
                {
                    std::lock_guard<std::mutex> status_lock(m_sensor_status_lock);
                    m_sensor_status.status = 0; // stopped
                }
                LOG_INFO("stop success!");
            }
            else
            {
                {
                    std::lock_guard<std::mutex> status_lock(m_sensor_status_lock);
                    m_sensor_status.status = 2; // error
                }
                LOG_ERROR("stop failed!");
            }
        }
        break;
        case SENSOR_CMD::CMD_GET_VERSION:
        {
            std::lock_guard<std::mutex> lock(m_sensor_info_lock);
            memcpy(m_sensor_info.version, body_buf, 6);
        }
        break;
        case SENSOR_CMD::CMD_RESET_ZERO:
        {
            char reset_zero_flag = body_buf[0];
            if (reset_zero_flag == 0x01)
            {
                LOG_INFO("reset zero success!");
            }
            else
            {
                LOG_ERROR("reset zero failed!");
            }
        }
        break;
        default:
        {
            LOG_ERROR("unprocessed response for cmd=%d", static_cast<int>(cmd));
        }
        break;
        }
    }
}

bool HypersenProxy::CheckComConnection()
{
    try
    {
        if (!m_com)
        {
            m_com = new BufferedAsyncSerial();
        }
        if (!m_com->isOpen())
        {
            m_com->open(m_com_name, GetBaudRate(m_baud_rate), boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::even));
        }
        if (m_com->errorStatus())
        {
            DestroyComConnection();
            m_is_sensor_ok = false;
            return false;
        }
        m_is_sensor_ok = true;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(u8"open com: %s failed, %s", m_com_name.c_str(), e.what());
    }
    m_is_sensor_ok = false;
    return false;
}

void HypersenProxy::DestroyComConnection()
{
    if (m_com)
    {
        try
        {
            m_com->close();
        }
        catch (...)
        {
        }
        delete m_com;
        m_com = nullptr;
    }
    m_is_sensor_ok = false;
    // reset sensor status
    {
        std::lock_guard<std::mutex> lock(m_sensor_status_lock);
        m_sensor_status.status = 0; // stopped
    }
    // reset sensor info
    {
        std::lock_guard<std::mutex> lock(m_sensor_info_lock);
        m_sensor_info = {0};
    }
    // reset current data
    {
        std::lock_guard<std::mutex> lock(m_cur_data_lock);
        m_cur_data = {0};
    }
}