#pragma once
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <ur_rtde/rtde_control_interface.h>
#include <ur_rtde/rtde_receive_interface.h>
#include <ur_rtde/dashboard_client.h>
#include <ur_rtde/robotiq_gripper.h>
#include "URDef.h"

class URManager
{
public:
    URManager();
    ~URManager();

    bool Connect(const std::string &hostname);
    void Disconnect();
    bool IsConnected() const;

    bool PowerOnRobot();
	bool PowerOffRobot();
	bool ShutdownRobot();

    bool MoveHome(double speed = 0.5, double acceleration = 0.5);
    bool MoveJ(const std::vector<double> &joint_positions, double speed = 0.5, double acceleration = 0.5);
    bool MoveL(const std::vector<double> &cartesian_pose, double speed = 0.5, double acceleration = 0.5);
    bool ServoL(ST_URServoL &servo_l);

    /////////////////////////////////////////////////////////////////////////////////////////
    bool ConnectGripper(const std::string &hostname);
    void DisconnectGripper();
    bool IsGripperConnected() const;

    int OpenGripper();
    int CloseGripper();
    int MoveGripper(float position, float speed = -1.0f, float force = -1.0f, bool wait_for_completion = false);

    ST_URDataFrame GetURDataFrame()
    {
        std::lock_guard<std::mutex> lock(m_data_frame_mutex);
        return m_data_frame;
    }
    ST_RobotiqGripperDataFrame GetGripperDataFrame()
    {
        std::lock_guard<std::mutex> lock(m_gripper_data_frame_mutex);
        return m_gripper_data_frame;
    }

protected:
    bool ReadURDataFrame(ST_URDataFrame &data_frame);
    bool ReadGripperDataFrame(ST_RobotiqGripperDataFrame &gripper_data_frame);
    void ThreadInfoFunc();

protected:
    std::string m_hostname;
    std::vector<double> m_home_position = {0.0, -1.57, 0, -1.57, 0.0, 0.0};

    std::shared_ptr<ur_rtde::RTDEControlInterface> m_control_interface;
    std::shared_ptr<ur_rtde::RTDEReceiveInterface> m_receive_interface;
    std::shared_ptr<ur_rtde::RobotiqGripper> m_gripper;

    std::thread m_thread_info;
    std::atomic<bool> m_exit_flag{false};

    std::mutex m_data_frame_mutex;
    ST_URDataFrame m_data_frame = {0};

    std::mutex m_gripper_data_frame_mutex;
    ST_RobotiqGripperDataFrame m_gripper_data_frame = {0};
};
