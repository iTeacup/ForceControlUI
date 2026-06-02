#pragma once
#include <stdint.h>

#pragma pack(push, 4)

#define HYPERSEN_SENSOR_DOF 6

struct ST_HypersenSensorInfo
{
    uint16_t dev_id; // 设备ID
    char version[7]; // 固件生成日期(年，月，日)，固件版本（主版本，小版本，修订号）
};

typedef struct
{
    char code;                       // 异常数据指示，0 表示数据正常、0xFF 表示数据出现异常
    float data[HYPERSEN_SENSOR_DOF]; // FxFyFz(单位：N),MxMyMz(单位：Nm)
} ST_HypersenSensorData;

typedef struct
{
    char status; // 0=stopped, 1=reading, 2=error
} ST_HypersenSensorStatus;

#pragma pack(pop)
