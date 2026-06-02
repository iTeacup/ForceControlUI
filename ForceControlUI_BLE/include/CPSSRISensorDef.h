#pragma once
#include <CPSAPI/CPSDef.h>

enum SapmlingRate
{
    S_15 = 0,
    S_30,
    S_45,
    S_60,
    S_120,
    S_240
};

inline int GetSampRate(SapmlingRate samprate)
{
    switch (samprate)
    {
    case S_15:
        return 15;
    case S_30:
        return 30;
    case S_45:
        return 45;
    case S_60:
        return 60;
    case S_120:
        return 120;
    case S_240:
        return 240;
    default:
        return 60;
    }
}

#pragma pack(push, 4)
////////////////////////////////消息定义////////////////////////////////////////
// 传感器重置零消息
#define MSG_REQ_SENSOR_RESET_ZERO 0x701

// 传感器数据消息
#define MSG_SRI_SENSOR_DATA 0x702
typedef struct
{
    float fx_main;
    float fy_main;
    float fz_main;
    float mx_main;
    float my_main;
    float mz_main;
} ST_SRISensorData;

#pragma pack(pop)
