#pragma once
#include <stdint.h>

typedef struct
{
    // 机器人运行时间，单位：s
    double running_time;
    // 机器人当前状态:
    /*  *-1 = ROBOT_MODE_NO_CONTROLLER
     *0 = ROBOT_MODE_DISCONNECTED
     * 1 = ROBOT_MODE_CONFIRM_SAFETY
     * 2 = ROBOT_MODE_BOOTING
     * 3 = ROBOT_MODE_POWER_OFF
     * 4 = ROBOT_MODE_POWER_ON
     * 5 = ROBOT_MODE_IDLE
     * 6 = ROBOT_MODE_BACKDRIVE
     * 7 = ROBOT_MODE_RUNNING
     * 8 = ROBOT_MODE_UPDATING_FIRMWARE
     */
    int robot_mode;
    // 机器人安全状态
    /*
        SAFETY_MODE_UNDEFINED_SAFETY_MODE	11
        SAFETY_MODE_VALIDATE_JOINT_ID	10
        SAFETY_MODE_FAULT	9
        SAFETY_MODE_VIOLATION	8
        SAFETY_MODE_ROBOT_EMERGENCY_STOP	7
        SAFETY_MODE_SYSTEM_EMERGENCY_STOP	6
        SAFETY_MODE_SAFEGUARD_STOP	5
        SAFETY_MODE_RECOVERY	4
        SAFETY_MODE_PROTECTIVE_STOP	3
        SAFETY_MODE_REDUCED	2
        SAFETY_MODE_NORMAL	1
    */
    int safety_mode;
    // 机器人末端笛卡尔坐标[x,y,z,rx,ry,rz]。x,y,z单位为m，rx,ry,rz单位为弧度
    double cartesian_info[6];
    // 机器人关节角度，[base, shoulder, elbow, wrist1, wrist2, wrist3], 单位为弧度
    double joint_info[6];
    // 机器人末端力和扭矩
    double tcp_force_info[6];
    // 机器人关节速度
    double joint_speed[6];
    // 机器人输出IO, 0-7: Standard, 8-15: Configurable, 16-17: Tool
    uint64_t actual_digital_output_bits;
} ST_URDataFrame;

typedef struct
{
    bool is_active;         // 是否连接
    float position;        // gripper position
    float closed_position; // gripper closed position
    float open_position;   // gripper open position
    int object_status;     // gripper object status
    /*
     * 0 = MOVING gripper is opening or closing
     * 1 = STOPPED_OUTER_OBJECT outer object detected while opening the gripper
     * 2 = STOPPED_INNER_OBJECT inner object detected while closing the gripper
     * 3 = AT_DEST requested target position reached - no object detected
     */
    int fault_code; // gripper fault code 0=ok
} ST_RobotiqGripperDataFrame;

typedef struct
{
    double pos[6];       // tool target pose
    double speed;        // tool speed [m/s]
    double acceleration; // tool acceleration [m/s^2]
    double time;
    double lookahead_time;
    double gain;
} ST_URServoL;
