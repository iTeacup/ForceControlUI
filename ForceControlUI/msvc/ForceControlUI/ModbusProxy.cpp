#include "ModbusProxy.h"
#include "Logger.h"
#include <chrono> // Ensure this header is included for std::chrono_literals

using namespace std::chrono_literals; // Add this line to enable chrono literals like 1s

#define OP_MIN_INTERVAL_MS 1

ModbusProxy::ModbusProxy()
{
}

ModbusProxy::~ModbusProxy()
{
    Disconnect();
}

bool ModbusProxy::Connect(const std::string &com, EMBBaudRate baudrate)
{
    Disconnect();

    int baud = GetBaudRate(baudrate);
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_ctx = modbus_new_rtu(com.c_str(), baud, 'N', 8, 1);
    if (m_ctx == NULL)
        return false;

#if 0
    modbus_set_debug(m_ctx, 1);
#endif

    // modbus_set_slave(m_ctx, slave_id);
    modbus_set_response_timeout(m_ctx, 0, 50000); // 设置超时50ms
    if (modbus_connect(m_ctx) == -1)
    {
        modbus_free(m_ctx);
        m_ctx = nullptr;
        return false;
    }
    m_is_connected = true;
    StartTimedQueryThread();
    return true;
}

void ModbusProxy::Disconnect()
{
    StopTimedQueryThread();
    m_active_slave_id.store(-1);
    ClearSlaveIDQueryList();

    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    if (m_ctx)
    {
        modbus_close(m_ctx);
        modbus_free(m_ctx);
        m_ctx = nullptr;
    }
    m_is_connected = false;
}

bool ModbusProxy::IsConnected()
{
    return m_is_connected;
}

void ModbusProxy::StartTimedQueryThread()
{
    if (m_query_thread.joinable())
    {
        return;
    }
    m_query_run_flag = true;
    m_query_thread = std::move(std::thread(&ModbusProxy::TimeQueryFunc, this));
}

void ModbusProxy::StopTimedQueryThread()
{
    m_query_run_flag = false;
    if (m_query_thread.joinable())
    {
        m_query_thread.join();
    }
}

void ModbusProxy::RegisterDataCallback(OnRegDataCB cb)
{
    std::lock_guard<std::mutex> lock(m_vec_cbs_lock);
    m_vec_cbs.push_back(cb);
}

void ModbusProxy::SetQueryTimeMs(int query_ms)
{
    m_query_ms = query_ms;
}

int ModbusProxy::GetQueryTimeMs()
{
    return m_query_ms;
}

void ModbusProxy::SetActiveSlaveId(int slave_id)
{
    m_active_slave_id.store(slave_id);
    // 重新设置modbus上下文的从站ID
    {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        modbus_set_slave(m_ctx, m_active_slave_id.load());
    }
	//ClearSlaveIDQueryList();
    AddSlaveIDtoQueryList(slave_id);
}

int ModbusProxy::GetActiveSlaveId() const
{
    return m_active_slave_id.load();
}

void ModbusProxy::AddSlaveIDtoQueryList(int slave_id)
{
    std::lock_guard<std::mutex> lock(m_slave_id_set_mutex);
    m_slave_id_set.insert(slave_id);
}

void ModbusProxy::RemoveSlaveIDFromQueryList(int slave_id)
{
    std::lock_guard<std::mutex> lock(m_slave_id_set_mutex);
    m_slave_id_set.erase(slave_id);
}

void ModbusProxy::ClearSlaveIDQueryList()
{
    std::lock_guard<std::mutex> lock(m_slave_id_set_mutex);
    m_slave_id_set.clear();
}

bool ModbusProxy::ReadHoldingRegisters(int addr, int nregs, uint16_t *data)
{
    if (m_ctx == nullptr || !m_is_connected || m_active_slave_id <= 0)
        return false;

    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    int ret = modbus_read_registers(m_ctx, addr, nregs, data);
    if (ret == -1)
    {
        // 读取失败
        return false;
    }
    if (ret != nregs)
    {
        // 读取的寄存器数量不匹配
        return false;
    }
    // 读取成功
    return true;
}

bool ModbusProxy::WriteSingleRegister(int addr, uint16_t data)
{
    if (m_ctx == nullptr || !m_is_connected || m_active_slave_id <= 0)
        return false;

    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    int ret = modbus_write_register(m_ctx, addr, data);
    if (ret == -1)
    {
        // 写入失败
        return false;
    }
    if (ret != 1)
    {
        // 写入的寄存器数量不匹配
        return false;
    }
    // 写入成功
    return true;
}

bool ModbusProxy::WriteMultipleRegisters(int addr, int nregs, const uint16_t *data)
{
    if (m_ctx == nullptr || !m_is_connected || m_active_slave_id <= 0)
        return false;

    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    int ret = modbus_write_registers(m_ctx, addr, nregs, data);
    if (ret == -1)
    {
        // 写入失败
        return false;
    }
    if (ret != nregs)
    {
        // 写入的寄存器数量不匹配
        return false;
    }
    // 写入成功
    return true;
}

bool ModbusProxy::ReadInputRegisters(int addr, int nregs, uint16_t *data)
{
    if (m_ctx == nullptr || !m_is_connected || m_active_slave_id <= 0)
        return false;

    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    int ret = modbus_read_input_registers(m_ctx, addr, nregs, data);
    if (ret == -1)
    {
        // 读取失败, 打印错误码
        LOG_ERROR(u8"[%d]Modbus read input registers failed: %s", m_active_slave_id.load(), modbus_strerror(errno));
        return false;
    }
    if (ret != nregs)
    {
        // 读取的寄存器数量不匹配
        return false;
    }
    // 读取成功
    return true;
}

bool ModbusProxy::QueryVersionInfo(version_info_t &info)
{
    int addr = MB_INPUT_VERSION_REG_ADDR;
    int nregs = MB_INPUT_VERSION_REG_NUM;
    if (!ReadInputRegisters(addr, nregs, reinterpret_cast<uint16_t *>(&info)))
    {
        UI_ERROR(u8"查询版本信息失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::QuerySystemStatus(system_status_t &status)
{
    int addr = MB_INPUT_STATUS_REG_ADDR;
    int nregs = MB_INPUT_STATUS_REG_NUM;
    if (!ReadInputRegisters(addr, nregs, reinterpret_cast<uint16_t *>(&status)))
    {
        UI_ERROR(u8"查询系统状态失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::QuerySystemSettings(system_settings_t &settings)
{
    int addr = MB_HOLD_SYS_SETTINGS_REG_ADDR;
    int nregs = MB_HOLD_SYS_SETTINGS_REG_NUM;
    if (!ReadHoldingRegisters(addr, nregs, reinterpret_cast<uint16_t *>(&settings)))
    {
        UI_ERROR(u8"查询系统设置失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::SaveSystemSettings(const system_settings_t&settings)
{
    int addr = MB_HOLD_SYS_SETTINGS_REG_ADDR;
    int nregs = MB_HOLD_SYS_SETTINGS_REG_NUM;
    if (!WriteMultipleRegisters(addr, nregs, reinterpret_cast<const uint16_t *>(&settings)))
    {
        UI_ERROR(u8"保存系统设置失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::RestartSystem()
{
    run_cmd_t cmd = { 0 };
    cmd.run_flag = FLAG_SYSTEM_REBOOT;
    int addr = MB_HOLD_RUN_CMD_REG_ADDR;
    return WriteSingleRegister(addr, cmd.run_flag); // 写入保持寄存器
}

bool ModbusProxy::QueryHomotorCmds(homotor_cmds_t &cmds)
{
    int addr = MB_HOLD_HOMOTOR_CMD_REG_ADDR;
    int nregs = MB_HOLD_HOMOTOR_CMD_REG_NUM;
    if (!ReadHoldingRegisters(addr, nregs, reinterpret_cast<uint16_t *>(&cmds)))
    {
        UI_ERROR(u8"查询中空电机命令失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::SetHomotorCmds(const homotor_cmds_t &cmds)
{
    int addr = MB_HOLD_HOMOTOR_CMD_REG_ADDR;
    int nregs = MB_HOLD_HOMOTOR_CMD_REG_NUM;
    if (!WriteMultipleRegisters(addr, nregs, reinterpret_cast<const uint16_t *>(&cmds)))
    {
        UI_ERROR(u8"设置中空电机命令失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::QueryLamotorCmds(lamotor_cmds_t &cmds)
{
    int addr = MB_HOLD_LAMOTOR_CMD_REG_ADDR;
    int nregs = MB_HOLD_LAMOTOR_CMD_REG_NUM;
    if (!ReadHoldingRegisters(addr, nregs, reinterpret_cast<uint16_t *>(&cmds)))
    {
        UI_ERROR(u8"查询因时电机命令失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::SetLamotorCmds(const lamotor_cmds_t &cmds)
{
    int addr = MB_HOLD_LAMOTOR_CMD_REG_ADDR;
    int nregs = MB_HOLD_LAMOTOR_CMD_REG_NUM;
    if (!WriteMultipleRegisters(addr, nregs, reinterpret_cast<const uint16_t *>(&cmds)))
    {
        UI_ERROR(u8"设置因时电机命令失败!");
        return false;
    }
    return true;
}

void ModbusProxy::TimeQueryFunc()
{
    bool success = false;
    adc_info_t adc_info = {0};
    while (m_query_run_flag)
    {
        if (!m_ctx || !m_is_connected || m_active_slave_id <= 0)
        {
            std::this_thread::sleep_for(1s);
            continue;
        }
        if (m_pause_query.load())
        {
            std::this_thread::sleep_for(100ms);
            continue;
        }
#if 0
        // 开始计时
        auto t1 = std::chrono::high_resolution_clock::now();
#endif
        std::set<int> slave_id_set_copy;
        {
            std::lock_guard<std::mutex> lock(m_slave_id_set_mutex);
            slave_id_set_copy = m_slave_id_set;
        }
        std::vector<OnRegDataCB> vec_cbs_copy;
        {
            std::lock_guard<std::mutex> lock(m_vec_cbs_lock);
            vec_cbs_copy = m_vec_cbs;
        }
        for (int slave_id : slave_id_set_copy)
        {
            // 设置从站ID
            SetActiveSlaveId(slave_id);
            // 读取输入寄存器数据
            success = QueryAdcinfo(adc_info);
            if (success)
            {                
                // 触发回调函数
                for (auto &cb : vec_cbs_copy)
                {
                    if (cb)
                    {
                        cb(slave_id, &adc_info);
                    }
                }
            }
            // 按m_query_ms间隔休眠
            //std::this_thread::sleep_for(std::chrono::milliseconds(m_query_ms));
        }
#if 0
        // 结束计时
        auto t2 = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        LOG_INFO(u8"Modbus read takes: %lldms", duration_ms);
#endif
        // 按m_query_ms间隔休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(m_query_ms));
    }
}

bool ModbusProxy::QueryAdcinfo(adc_info_t& adc)
{
    int addr = MB_INPUT_ADC_INFO_REG_ADDR;
    int nregs = MB_INPUT_ADC_INFO_REG_NUM;
    if (!ReadInputRegisters(addr, nregs, reinterpret_cast<uint16_t*>(&adc)))
    {
        UI_ERROR(u8"查询ADC信息失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::QueryHomotorInfo(homotor_info_t info[HO_MOTOR_NUM])
{
    int addr = MB_INPUT_HOMOTOR_INFO_REG_ADDR;
    int nregs = MB_INPUT_HOMOTOR_INFO_REG_NUM;
    if (!ReadInputRegisters(addr, nregs, reinterpret_cast<uint16_t *>(info)))
    {
        UI_ERROR(u8"查询中空电机信息失败!");
        return false;
    }
    return true;
}

bool ModbusProxy::QueryLamotorInfo(lamotor_info_t info[LA_MOTOR_NUM])
{
    int addr = MB_INPUT_LAMOTOR_INFO_REG_ADDR;
    int nregs = MB_INPUT_LAMOTOR_INFO_REG_NUM;
    if (!ReadInputRegisters(addr, nregs, reinterpret_cast<uint16_t *>(info)))
    {
        UI_ERROR(u8"查询因时电机信息失败!");
        return false;
    }
    return true;
}
