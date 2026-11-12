#include "URManager.h"
#include "Logger.h"
#include <cmath>
#include <stdexcept>

class URManager::SingleJointLock {
    static void Require(bool ok,const char* message) { if(!ok) throw std::invalid_argument(message); }
    std::vector<double> held;
    int selected;
public:
    SingleJointLock(const std::vector<double>& initial,int joint):held(initial),selected(joint) {
        Require(held.size()==6&&joint>=0&&joint<6,"Invalid single-joint session");
        for(double q:held) Require(std::isfinite(q),"Invalid held joint");
    }
    std::vector<double> Target(double radians) const {
        Require(std::isfinite(radians),"Invalid selected joint target");
        auto q=held;q[selected]=radians;return q;
    }
    void Validate(const std::vector<double>& q) const {
        Require(q.size()==6,"Expected exactly six targets");
        for(int i=0;i<6;++i) {
            Require(std::isfinite(q[i]),"Invalid joint target");
            if(i!=selected) Require(q[i]==held[i],"Unselected joint target changed");
        }
    }
};

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
    EndJointTeleop();
    m_exit_flag = true;
    if (m_thread_info.joinable()) m_thread_info.join();
    std::lock_guard<std::mutex> control(m_control_mutex);
    try
    {
        if (m_control_interface) m_control_interface->disconnect();
        if (m_receive_interface) m_receive_interface->disconnect();
    }
    catch (const std::exception& e) { LOG_ERROR("UR disconnect: {}", e.what()); }
    m_control_interface.reset();
    m_receive_interface.reset();
    std::lock_guard<std::mutex> data(m_data_frame_mutex);
    m_data_frame = {};
    m_frame_updated = {};
    m_speed_scaling = std::numeric_limits<double>::quiet_NaN();
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
    EndJointTeleop();
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
    EndJointTeleop();
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
    std::lock_guard<std::mutex> control(m_control_mutex);
    if (m_joint_teleop) return false;
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
    std::lock_guard<std::mutex> control(m_control_mutex);
    if (m_joint_teleop) return false;
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
    std::lock_guard<std::mutex> control(m_control_mutex);
    if (m_joint_teleop) return false;
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
    std::lock_guard<std::mutex> control(m_control_mutex);
    if (m_joint_teleop) return false;
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
    data_frame = {};
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
            if (cartesian_info.size() != 6) return false;
            for (size_t i = 0; i < 6 && i < cartesian_info.size(); ++i)
            {
                data_frame.cartesian_info[i] = cartesian_info[i];
            }
            std::vector<double> joint_info = m_receive_interface->getActualQ();
            if (joint_info.size() != 6) return false;
            for (size_t i = 0; i < 6 && i < joint_info.size(); ++i)
            {
                data_frame.joint_info[i] = joint_info[i];
            }
            std::vector<double> tcp_force_info = m_receive_interface->getActualTCPForce();
            if (tcp_force_info.size() != 6) return false;
            for (size_t i = 0; i < 6 && i < tcp_force_info.size(); ++i)
            {
                data_frame.tcp_force_info[i] = tcp_force_info[i];
            }
            std::vector<double> joint_speed = m_receive_interface->getActualQd();
            if (joint_speed.size() != 6) return false;
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
            ST_URDataFrame data_frame{};
            if (ReadURDataFrame(data_frame))
            {
                double scaling=std::numeric_limits<double>::quiet_NaN();
                try {scaling=m_receive_interface->getSpeedScalingCombined();}
                catch (const std::exception&) {} // Optional telemetry must not invalidate joint feedback.
                std::lock_guard<std::mutex> lock(m_data_frame_mutex);
                m_speed_scaling=scaling;
                if (data_frame.running_time != m_data_frame.running_time)
                    m_frame_updated = std::chrono::steady_clock::now();
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

// Exclusive joint-servo session, shared by both robot windows.
bool URManager::RobotFeedback(ST_URDataFrame& frame)
{
    std::lock_guard<std::mutex> data(m_data_frame_mutex);
    frame = m_data_frame;
    if (std::chrono::duration<double>(std::chrono::steady_clock::now()-m_frame_updated).count() > 0.25)
        return false;
    for (int i=0; i<6; ++i)
        if (!std::isfinite(frame.joint_info[i]) || !std::isfinite(frame.joint_speed[i])) return false;
    return true;
}

bool URManager::SpeedScaling(double& value)
{
    std::lock_guard<std::mutex> data(m_data_frame_mutex);
    value=m_speed_scaling;
    return std::chrono::duration<double>(std::chrono::steady_clock::now()-m_frame_updated).count()<=0.25 &&
        std::isfinite(value) && value>=0 && value<=1;
}

bool URManager::TeleopFeedback(ST_URDataFrame& frame)
{
    return RobotFeedback(frame) && frame.robot_mode == 7 &&
        (frame.safety_mode == 1 || frame.safety_mode == 2);
}

bool URManager::BeginJointTeleop(int selected, std::vector<double>& initial)
{
    std::lock_guard<std::mutex> control(m_control_mutex);
    if (selected<0 || selected>=6 || m_joint_teleop || !IsConnected()) return false;
    ST_URDataFrame frame{};
    if (!TeleopFeedback(frame)) return false;
    for (double speed : frame.joint_speed) if (std::abs(speed)>0.01) return false;
    try
    {
        if (!m_control_interface->isSteady()) return false;
        initial.assign(frame.joint_info, frame.joint_info+6);
        if (!m_control_interface->isJointsWithinSafetyLimits(initial)) return false;
        m_teleop_lock = std::make_unique<SingleJointLock>(initial, selected);
        m_teleop_servo_started = false;
        m_joint_teleop = true;
        return true;
    }
    catch (const std::exception& e) { LOG_ERROR("Teleop start: {}", e.what()); return false; }
}

bool URManager::ServoJointTeleop(double selectedRadians, double period)
{
    std::lock_guard<std::mutex> control(m_control_mutex);
    ST_URDataFrame frame{};
    if (!m_joint_teleop || !m_teleop_lock || !IsConnected() || !TeleopFeedback(frame)) return false;
    try
    {
        // Only one scalar is accepted from callers; all other components are reconstructed here.
        auto target=m_teleop_lock->Target(selectedRadians);
        m_teleop_lock->Validate(target);
        if (!m_control_interface->isJointsWithinSafetyLimits(target)) return false;
        m_teleop_servo_started = true;
        return m_control_interface->servoJ(target, 0, 0, period, 0.1, 200);
    }
    catch (const std::exception& e) { LOG_ERROR("Teleop servo: {}", e.what()); return false; }
}

void URManager::EndJointTeleop()
{
    std::lock_guard<std::mutex> control(m_control_mutex);
    if (!m_joint_teleop.exchange(false)) return;
    m_teleop_lock.reset();
    const bool hadServo = m_teleop_servo_started;
    m_teleop_servo_started = false;
    try { if (hadServo && IsConnected()) m_control_interface->servoStop(0.5); }
    catch (const std::exception& e) { LOG_ERROR("Teleop stop: {}", e.what()); }
}
