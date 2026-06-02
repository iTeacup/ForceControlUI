#pragma once
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <vector>
#include <set>
#include <modbus/modbus.h>
#include "ModbusDef.h"
#include "common_datadef.h"

typedef std::function<void(int, const adc_info_t *)> OnRegDataCB;

class ModbusProxy
{
public:
    ModbusProxy();
    ~ModbusProxy();

    bool Connect(const std::string &com, EMBBaudRate baudrate);
    void Disconnect();
    bool IsConnected();

    void StartTimedQueryThread();
    void StopTimedQueryThread();

    void SetPauseQuery(bool pause) { m_pause_query.store(pause); }
    bool IsPauseQuery() const { return m_pause_query.load(); }

    void RegisterDataCallback(OnRegDataCB cb);

    void SetQueryTimeMs(int query_ms);
    int GetQueryTimeMs();

    void SetActiveSlaveId(int slave_id);
    int GetActiveSlaveId() const;

    void AddSlaveIDtoQueryList(int slave_id);
    void RemoveSlaveIDFromQueryList(int slave_id);
    void ClearSlaveIDQueryList();

    bool ReadHoldingRegisters(int addr, int nregs, uint16_t *data);
    bool WriteSingleRegister(int addr, uint16_t data);
    bool WriteMultipleRegisters(int addr, int nregs, const uint16_t *data);
    bool ReadInputRegisters(int addr, int nregs, uint16_t *data);

public:
    bool QuerySystemStatus(system_status_t &status);
    bool QueryVersionInfo(version_info_t &info);
    bool QueryAdcinfo(adc_info_t& adc);
    bool QueryHomotorInfo(homotor_info_t info[HO_MOTOR_NUM]);
    bool QueryLamotorInfo(lamotor_info_t info[LA_MOTOR_NUM]);
    
    bool RestartSystem();
    bool QueryHomotorCmds(homotor_cmds_t &cmds);
    bool SetHomotorCmds(const homotor_cmds_t &cmds);
    bool QueryLamotorCmds(lamotor_cmds_t &cmds);
    bool SetLamotorCmds(const lamotor_cmds_t &cmds);
    bool QuerySystemSettings(system_settings_t &settings);
    bool SaveSystemSettings(const system_settings_t&settings);
    
protected:
    void TimeQueryFunc();

protected:
    std::atomic_int m_active_slave_id = 0;

    std::mutex m_ctx_mutex;
    modbus_t *m_ctx = nullptr;
    bool m_is_connected = false;

    std::chrono::high_resolution_clock::time_point m_op_start = std::chrono::high_resolution_clock::now();

    std::mutex m_slave_id_set_mutex;
    std::set<int> m_slave_id_set;

    std::thread m_query_thread;
    std::atomic_bool m_query_run_flag = false;
    std::atomic_bool m_pause_query = false;
    std::atomic_int32_t m_query_ms = 20;

    std::mutex m_vec_cbs_lock;
    std::vector<OnRegDataCB> m_vec_cbs;
};
