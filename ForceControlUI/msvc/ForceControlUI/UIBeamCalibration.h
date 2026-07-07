#pragma once
#pragma once
#define BEAM_NUM 2
#define BEAM_LENGTH_NUM 10
#define POINTS_NUM 2
#define CALIBRATION_STRAIN_CHANNEL_NUM 8 // ADC通道数
#define EMB_MAX_COM_NUM 40
#define _PI 3.14159265358979323846f

#include "UIBaseWindow.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>
#include "ModbusProxy.h"
#include "common_datadef.h"
#include "UIPlotDef.h"
#include "UIHypersenMonitor.h"
#include <fmt/format.h>
#include "URManager.h"
#include <vector>

struct ST_CalibrationData
{
    float tcp[6] = { 0 };//Robot pose
    float com_data[CALIBRATION_STRAIN_CHANNEL_NUM] = { 0 };
};

struct ST_BeamBleData
{
    uint32_t timestamp = 0;
    float channel_mv[CALIBRATION_STRAIN_CHANNEL_NUM / BEAM_NUM] = { 0.0f };
};


class UIBeamCalibration : public UIBaseWindow
{
public:
    UIBeamCalibration(UIMainWindowBase* main_win, const char* title);
    ~UIBeamCalibration();

    virtual void Draw();
    virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
    virtual const char* GetShowShortCut() { return "Ctrl+2"; }

protected:
    //void OnRegDataCB(const umb_holdregs_t* holdregs, const umb_inputregs_t* inputregs);

    void CalibrationThreadFunc();
    bool ParseCSVFile(const std::string& filename);

protected:
    enum class EBleState
    {
        Disconnected,
        Scanning,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };

    struct SBleBeamDevice
    {
        uint32_t id = 0;
        char display_name[64] = "BLE设备";
        char device_name[128] = "nRF52840_01";
        char service_uuid[64] = "19B10000-E8F2-537E-4F6C-D104768A1214";
        char characteristic_uuid[64] = "19B10002-E8F2-537E-4F6C-D104768A1214";

        mutable std::mutex state_lock;
        EBleState state = EBleState::Disconnected;
        std::string state_msg = u8"未连接";
        std::string last_packet_text;
        uint64_t packet_count = 0;

        std::atomic_bool stop_ble = false;
        std::thread ble_thread;

        ST_BeamBleData current_data;
        std::mutex data_lock;
        std::chrono::steady_clock::time_point last_packet_time = std::chrono::steady_clock::now();
    };

    void InitBleDevices();
    void DrawBleConfig();
    void DrawBleDevice(size_t index, SBleBeamDevice* device);
    bool IsBleBusyOrConnected(const SBleBeamDevice& device) const;
    bool IsBleConnected(const SBleBeamDevice& device) const;
    void StartBleConnect(SBleBeamDevice* device);
    void StopBleConnect(SBleBeamDevice* device);
    void StopAllBleDevices();
    void BleThreadFunc(SBleBeamDevice* device);
    void OnBlePacketReceived(SBleBeamDevice* device, const std::string& packet);
    bool ParseBlePacket(const std::string& packet, ST_BeamBleData& data);
    void SetBleState(SBleBeamDevice* device, EBleState state, const std::string& message);
    void CopyBleSensorData(float sensors[CALIBRATION_STRAIN_CHANNEL_NUM]);
    //ModbusProxy* m_modbus = nullptr;
    //HypersenProxy* m_hypersen_proxy = nullptr;

    ImVector<int> m_display_line_offsets; // 行起始偏移，首元素恒为 0

    /**
     * @brief 去掉字符串中的所有换行符（\n 和 \r）
     * @param s 输入字符串
     * @return 处理后的字符串
     */
    static std::string RemoveNewlines(const std::string& s);


    /**
     * @brief 拼接多个字符串，去掉各自内部换行符，整体末尾加一个换行符
     * @tparam Args 可变参数类型
     * @param args 多个字符串参数
     * @return 拼接后的字符串
     */
    template<typename... Args>
    static std::string JoinAndAddFinalNewline(Args&&... args);

    /**
     * @brief 解析逗号分隔的浮点数字符串，如 "600,500,321.5,321\n"
     * @param input 输入字符串
     * @return float数组
     */
    void ParseFloatArray(const std::string& input);
    
    float m_sensors[CALIBRATION_STRAIN_CHANNEL_NUM] = { 0 };
    //std::vector<float> m_sensors{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f , 0.0f , 0.0f , 0.0f , 0.0f };  // 5个0

    std::array<SBleBeamDevice, BEAM_NUM> m_ble_devices;

    //std::mutex m_mbinput_lock;
    //umb_inputregs_t m_mbinput = { 0 };
    //umb_holdregs_t m_mbhold = { 0 };

    float m1_ratio = 0.015f;
    float m2_ratio = 0.005f;

    int m_gripper_width_mm = 10; // 夹爪宽度mm

    ////Related to the input of calibration
    //float m_x_constraints = 10;
    //float m_y_constraints = 70;
    //float m_x_constraint_begin = 10;
    //float m_x_constraint_end = 20;
    //float m_y_constraint_begin = 72;
    //float m_y_constraint_end = 70;
    //float m_x_position_step = 1;
    //float m_y_position_step = 0.1;
    int m_temp_attempt = 1;
    int m_attempt_num = 3;
    /// The input of motors
    int m_m1_step = 0;
    int m_m2_step = 0;

    float m_wait_time_sec = 0.5; // 等待时间秒
    float m_ur_move_speed = 0.35f; // UR MoveL speed, m/s
    float m_ur_move_accel = 0.8f; // UR MoveL acceleration, m/s^2
    int m_motor1_begin = 2000;
    int m_motor1_end = 0;
    int m_motor2_begin = 0;
    int m_motor2_end = 2000;
    int m_step = 25;
    float m_XInitForce = 0;
    float m_XForce = 0;
    float m_YInitForce = 0;
    float m_YForce = 0;
    int m_gripper_aim_position_x = 10; //aim position of the gripper tip, unit mm
    int m_gripper_aim_position_y = 73; //aim position of the gripper tip, unit mm

    float m_x_begin = 30;
    float m_x_end = 10;
    float m_y_begin = 70;
    float m_y_end = 75;
    float m_position_step = 1;
    int m_force_control_process = 0;
    float fc_adc_data[ADC_CHANNEL_NUM];          // the adc data used for force control, which is initiated

    std::vector<float> Gripper_Parameters = { 45,14.5,12.5,0,-35,-45,-12.5,-28,28,-18,18,38,13,45,45,25,46,14 };
    std::atomic_bool m_calibration_run_flag = false;
    std::atomic_bool m_adjust_finish_flag = false; //Some steps in the calibration process need handle dealing, this flag aims to tell the system to continue after handle adjustment
    std::atomic_bool m_auto_adjust_flag = false;//Decide wether use the auto adjustment
   
    std::thread m_calibration_thread;
    std::thread m_positioning_thread;//used for position control of one finger

    

    std::mutex m_sensors_data_lock;

    std::mutex m_calibration_data_lock;
    std::vector<ST_CalibrationData> m_calibration_data; // 实验数据
    std::vector<int> m_motor_actuation_step;


    //Record of motors
    float m_m1 = 0;
    float m_m2 = 0;
    float m_m3 = 0;

    //Aim force
    float m_aim_x_force = 0;
    float m_aim_y_force = 0;

    using Pose = std::array<double, 6>;
    std::vector<Pose> m_calibration_aim_poses;

    //The name of the aim pose file
    std::string m_loaded_filename = "robot_poses.csv";



protected:
    std::vector<int> Motor_Position_Cal(float Aim_position_x, float Aim_position_y);

    char m_hostname[64] = { 0 }; // 机器人IP地址

    URManager m_ur;

    ST_URDataFrame m_ur_data = { 0 };
    ST_RobotiqGripperDataFrame m_gripper_data = { 0 };
};

// ========== 模板方法必须在头文件中实现 ==========

template<typename... Args>
std::string UIBeamCalibration::JoinAndAddFinalNewline(Args&&... args) {
    std::string result;

    // 预分配空间
    size_t total_size = (0 + ... + std::string(std::forward<Args>(args)).size());
    result.reserve(total_size + 1);

    // 折叠表达式：处理每个参数
    (result += ... += RemoveNewlines(std::forward<Args>(args)));

    result += '\n';
    return result;
}