#include "URManager.h"
#include "Logger.h"

URManager::URManager()
{
}

URManager::~URManager()
{
    Disconnect();
    DisconnectGripper();
}

bool URManager::Connect(const std::string &hostname)
{
    Disconnect();

    try
    {
        m_hostname = hostname;
        if (hostname.empty())
        {
            LOG_ERROR("Hostname cannot be empty");
            return false;
        }
        // Initialize interfaces
        if (!m_control_interface)
        {
            m_control_interface = std::make_shared<ur_rtde::RTDEControlInterface>(hostname);
        }

        if (!m_receive_interface)
        {
            m_receive_interface = std::make_shared<ur_rtde::RTDEReceiveInterface>(hostname);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to connect to UR: {}", e.what());
        return false;
    }
    m_exit_flag = false;
    m_thread_info = std::thread(&URManager::ThreadInfoFunc, this);
    return IsConnected();
}

void URManager::Disconnect()
{
    try
    {
        if (m_control_interface)
        {
            m_control_interface->disconnect();
        }
        if (m_receive_interface)
        {
            m_receive_interface->disconnect();
        }
        m_exit_flag = true;
        if (m_thread_info.joinable())
        {
            m_thread_info.join();
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to disconnect from UR: {}", e.what());
    }
}

bool URManager::IsConnected() const
{
    return m_control_interface && m_control_interface->isConnected() &&
           m_receive_interface && m_receive_interface->isConnected();
}

bool URManager::PowerOnRobot()
{
    try
    {
        ur_rtde::DashboardClient dashboard_client(m_hostname);
        dashboard_client.connect();
        if (dashboard_client.isConnected())
        {
            dashboard_client.powerOn();
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for the robot to power on
            dashboard_client.closePopup();
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for the popup to close
            dashboard_client.brakeRelease();
            dashboard_client.disconnect();
            return true;
        }
        else
        {
            LOG_ERROR("Failed to connect to Dashboard Client at {}", m_hostname);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to power on robot: {}", e.what());
    }
    return false;
}

bool URManager::PowerOffRobot()
{
    try
    {
        ur_rtde::DashboardClient dashboard_client(m_hostname);
        dashboard_client.connect();
        if (dashboard_client.isConnected())
        {
            dashboard_client.powerOff();
            dashboard_client.disconnect();
            return true;
        }
        else
        {
            LOG_ERROR("Failed to connect to Dashboard Client at {}", m_hostname);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to power off robot: {}", e.what());
    }
    return false;
}

bool URManager::ShutdownRobot()
{
    try
    {
        ur_rtde::DashboardClient dashboard_client(m_hostname);
        dashboard_client.connect();
        if (dashboard_client.isConnected())
        {
            Disconnect();
            DisconnectGripper();

            dashboard_client.shutdown();
            dashboard_client.disconnect();
            return true;
        }
        else
        {
            LOG_ERROR("Failed to connect to Dashboard Client at {}", m_hostname);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to shutdown robot: {}", e.what());
    }
    return false;
}

bool URManager::MoveHome(double speed, double acceleration)
{
    if (IsConnected())
    {
        try
        {
            return m_control_interface->moveJ(m_home_position, speed, acceleration, true);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Failed to move to home position: {}", e.what());
        }
    }
    else
    {
        LOG_ERROR("UR is not connected");
    }
    return false;
}

bool URManager::MoveJ(const std::vector<double> &joint_positions, double speed, double acceleration)
{
    if (IsConnected())
    {
        try
        {
            if(!m_control_interface->isJointsWithinSafetyLimits(joint_positions))
            {
                LOG_ERROR("Joint positions are out of safety limits");
                return false;
            }

            return m_control_interface->moveJ(joint_positions, speed, acceleration, true);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Failed to move to joint positions: {}", e.what());
        }
    }
    else
    {
        LOG_ERROR("UR is not connected");
    }
    return false;
}

bool URManager::MoveL(const std::vector<double> &cartesian_pose, double speed, double acceleration)
{
    if (IsConnected())
    {
        try
        {
            if(!m_control_interface->isPoseWithinSafetyLimits(cartesian_pose))
            {
                LOG_ERROR("Cartesian pose is out of safety limits");
                return false;
            }
            return m_control_interface->moveL(cartesian_pose, speed, acceleration, true);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Failed to move to cartesian pose: {}", e.what());
        }
    }
    else
    {
        LOG_ERROR("UR is not connected");
    }
    return false;
}

bool URManager::ServoL(ST_URServoL &servo_l)
{
    if (IsConnected())
    {
        try
        {
            std::vector<double> position(servo_l.pos, servo_l.pos + 6);
            if (!m_control_interface->isPoseWithinSafetyLimits(position))
            {
                LOG_ERROR("ServoL position is out of safety limits");
                return false;
            }
            return m_control_interface->servoL(position, servo_l.speed, servo_l.acceleration, servo_l.time,
                                               servo_l.lookahead_time, servo_l.gain);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Failed to perform ServoL: {}", e.what());
        }
    }
    else
    {
        LOG_ERROR("UR is not connected");
    }
    return false;
}

bool URManager::ConnectGripper(const std::string &hostname)
{
    try
    {
        if (!m_gripper)
        {
            m_gripper = std::make_shared<ur_rtde::RobotiqGripper>(hostname);

            m_gripper->connect();
            if (!m_gripper->isConnected())
            {
                LOG_ERROR("Failed to connect to Robotiq Gripper at {}", hostname);
                return false;
            }
            m_gripper->activate();                                                                       // Activate the gripper with auto-calibration
            m_gripper->setUnit(ur_rtde::RobotiqGripper::POSITION, ur_rtde::RobotiqGripper::UNIT_NORMALIZED); // Set unit to normalized
            m_gripper->setUnit(ur_rtde::RobotiqGripper::SPEED, ur_rtde::RobotiqGripper::UNIT_NORMALIZED);    // Set speed unit to normalized
            m_gripper->setUnit(ur_rtde::RobotiqGripper::FORCE, ur_rtde::RobotiqGripper::UNIT_NORMALIZED);    // Set force unit to normalized
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to connect to Robotiq Gripper: {}", e.what());
        return false;
    }
    return true;
}

void URManager::DisconnectGripper()
{
    if (m_gripper)
    {
        m_gripper->disconnect();
    }
}

bool URManager::IsGripperConnected() const
{
    return m_gripper && m_gripper->isConnected();
}

int URManager::OpenGripper()
{
    try
    {
        if (IsGripperConnected())
        {
            return m_gripper->open();
        }
        else
        {
            LOG_ERROR("Gripper is not connected");
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to open gripper: {}", e.what());
    }
    return -1;
}

int URManager::CloseGripper()
{
    try
    {
        if (IsGripperConnected())
        {
            return m_gripper->close();
        }
        else
        {
            LOG_ERROR("Gripper is not connected");
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to close gripper: {}", e.what());
    }
    return -1;
}

int URManager::MoveGripper(float position, float speed, float force, bool wait_for_completion)
{
    try
    {
        if (IsGripperConnected())
        {
            return m_gripper->move(position, speed, force,
                                   wait_for_completion ? ur_rtde::RobotiqGripper::WAIT_FINISHED : ur_rtde::RobotiqGripper::START_MOVE);
        }
        else
        {
            LOG_ERROR("Gripper is not connected");
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to move gripper: {}", e.what());
    }
    return -1;
}

bool URManager::ReadURDataFrame(ST_URDataFrame &data_frame)
{
    try
    {
        if (IsConnected())
        {
            data_frame.running_time = m_receive_interface->getTimestamp();
            data_frame.robot_mode = m_receive_interface->getRobotMode();
            data_frame.safety_mode = m_receive_interface->getSafetyMode();
            // 机器人未初始化时返回数据不正确
            if (data_frame.running_time < 0 || data_frame.robot_mode < 0 || data_frame.safety_mode < 0)
            {
                return false;
            }
            std::vector<double> cartesian_info = m_receive_interface->getActualTCPPose();
            for (size_t i = 0; i < 6 && i < cartesian_info.size(); ++i)
            {
                data_frame.cartesian_info[i] = cartesian_info[i];
            }
            std::vector<double> joint_info = m_receive_interface->getActualQ();
            for (size_t i = 0; i < 6 && i < joint_info.size(); ++i)
            {
                data_frame.joint_info[i] = joint_info[i];
            }
            std::vector<double> tcp_force_info = m_receive_interface->getActualTCPForce();
            for (size_t i = 0; i < 6 && i < tcp_force_info.size(); ++i)
            {
                data_frame.tcp_force_info[i] = tcp_force_info[i];
            }
            std::vector<double> joint_speed = m_receive_interface->getActualQd();
            for (size_t i = 0; i < 6 && i < joint_speed.size(); ++i)
            {
                data_frame.joint_speed[i] = joint_speed[i];
            }
            data_frame.actual_digital_output_bits = m_receive_interface->getActualDigitalOutputBits();
            return true;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to read UR data frame: {}", e.what());
    }
    return false;
}

bool URManager::ReadGripperDataFrame(ST_RobotiqGripperDataFrame &gripper_data_frame)
{
    try
    {
        if (IsGripperConnected())
        {
            gripper_data_frame.is_active = m_gripper->isActive();
            gripper_data_frame.position = m_gripper->getCurrentPosition();
            gripper_data_frame.closed_position = m_gripper->getClosedPosition();
            gripper_data_frame.open_position = m_gripper->getOpenPosition();
            gripper_data_frame.object_status = (int)m_gripper->objectDetectionStatus();
            gripper_data_frame.fault_code = m_gripper->faultStatus();
            return true;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to read Robotiq Gripper data frame: {}", e.what());
    }
    return false;
}

void URManager::ThreadInfoFunc()
{
    while (!m_exit_flag)
    {
        if (IsConnected())
        {
            ST_URDataFrame data_frame;
            if (ReadURDataFrame(data_frame))
            {
                std::lock_guard<std::mutex> lock(m_data_frame_mutex);
                m_data_frame = data_frame;
            }
        }
        if (IsGripperConnected())
        {
            ST_RobotiqGripperDataFrame gripper_data_frame;
            if (ReadGripperDataFrame(gripper_data_frame))
            {
                std::lock_guard<std::mutex> lock(m_gripper_data_frame_mutex);
                m_gripper_data_frame = gripper_data_frame;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Adjust the sleep duration as needed
    }
}
