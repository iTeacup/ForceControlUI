#pragma once
#include <CPSAPI/CPSDef.h>
#include "common_datadef.h"

#pragma pack(push, 4)

#define PIPE_ROBOT_CMD_HOMOTOR 0x1001
typedef struct
{
    homotor_cmds_t cmds; // 中空电机命令
} ST_CMD_PIPE_ROBOT_HOMOTOR;

#define PIPE_ROBOT_CMD_PWM_MOTOR 0x1002
typedef struct
{
    int speed;     // 速度，-100-100
} ST_CMD_PIPE_ROBOT_PWM_MOTOR;
#pragma pack(pop)
