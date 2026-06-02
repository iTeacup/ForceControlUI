#ifndef COMMON_DATADEF_H
#define COMMON_DATADEF_H
#include <stdint.h>

#if defined(_MSC_VER)
#pragma pack(push, 2)
#define PACKED2
#elif defined(__GNUC__) || defined(__clang__)
#define PACKED2 __attribute__((packed, aligned(2)))
#else
#pragma pack(push, 2)
#define PACKED2
#endif
//////////////////////////////////////////common/////////////////////////////////////////////
typedef struct PACKED2
{
    uint8_t version_major; // 主要版本号
    uint8_t version_minor; // 次要版本号
} version_info_t;

typedef struct PACKED2
{
    uint8_t sys_state;  // 当前系统状态
    uint8_t error_code; // 系统错误码
} system_status_t;
//////////////////////////////////////////motor/////////////////////////////////////////////
typedef struct PACKED2
{
    uint8_t motor_id;       // 电机ID
    uint8_t _pack0;         // 填充字节
    int32_t current_pos;    // 当前电机位置（单位：度）
    int16_t current_speed;  // 当前电机速度（单位：弧度/秒）
    int16_t current_torque; // 当前电机扭矩（单位：A）
} homotor_info_t;

typedef struct PACKED2
{
    uint8_t motor_id; // 电机ID
    uint8_t _pack0;   // 填充字节
    uint16_t target_pos;
    //    int16_t current_pos;
    //    int8_t temp;
    //    uint16_t current;
    //    int16_t force; // unit: grams
    //    int8_t error;
} lamotor_info_t;

//////////////////////////////////////////adc/////////////////////////////////////////////
#define ADC_CHANNEL_NUM 7 // ADC通道数
#define LA_MOTOR_NUM 3    // 因时电机数量
#define HO_MOTOR_NUM 3    // 中空电机数量
#define COM_CHANNEL_NUM 4 //The number of channels of one com
typedef struct PACKED2
{
    int16_t adc_dt;                         // 采样时间（单位：ms）
    uint16_t channel_data[ADC_CHANNEL_NUM]; // 通道数据（单位：0.1mV）
} adc_info_t;

/////////////COM/////////////
typedef struct PACKED2
{
    int16_t com_dt;                         // 采样时间（单位：ms）
    uint16_t channel_data[COM_CHANNEL_NUM]; // 通道数据（单位：0.1mV）
} com_info_t;

//////////////////////////////////////cmd////////////////////////////////////
#define FLAG_SYSTEM_REBOOT 0x01F
typedef struct PACKED2
{
    uint16_t run_flag;
} run_cmd_t;

typedef struct PACKED2
{
    uint8_t homotor_id;      // 中空电机ID
    uint8_t homotor_mode;    // 中空电机运动模式
    float homotor_pos_deg;   // 中空电机目标位置（单位：度）
    float homotor_speed_rad; // 中空电机目标速度（单位：弧度/秒）
    int homotor_pos_p;       // 中空电机位置控制比例系数
    int homotor_pos_d;       // 中空电机位置控制微分系数
    float homotor_current;   // 中空电机目标电流（单位：A）
} homotor_cmd_t;

typedef struct PACKED2
{
    uint8_t enable;                  // 中空电机使能标志
    uint8_t _pack0;                  // 填充字节
    homotor_cmd_t cmd[HO_MOTOR_NUM]; // 中空电机命令
} homotor_cmds_t;

typedef struct PACKED2
{
    uint8_t lamotor_id;          // 因时电机ID
    uint8_t lamotor_mode;        // 因时电机运动模式 0: enable, 1: pause 2: quick stop
    uint16_t lamotor_target_pos; // 因时电机目标位置
} lamotor_cmd_t;

typedef struct PACKED2
{
    uint8_t enable;                  // 因时电机使能标志
    uint8_t _pack0;                  // 填充字节
    lamotor_cmd_t cmd[LA_MOTOR_NUM]; // 因时电机命令
} lamotor_cmds_t;
/////////////////////////////////////sys config///////////////////////////////////////////////////////////
typedef struct PACKED2
{
    uint8_t ch_enable_mask; // 通道使能掩码
    uint8_t pga_gain;       // PGA增益
    uint16_t ch_filter_fs;  // 滤波器FS值
} adc_config_t;

// 任务启动掩码
#define TASK_LOG_MASK (1 << 0)
#define TASK_AD_MASK (1 << 1)
#define TASK_CAN_MASK (1 << 2)
#define TASK_HOMOTOR_MASK (1 << 3)
#define TASK_LAMOTOR_MASK (1 << 4)

typedef struct PACKED2
{
    // 任务启动掩码
    uint8_t task_mask;
    // 内存打印延时（单位：100毫秒）
    uint8_t mem_print_dt_100ms;
    // modbus id
    uint8_t mb_slave_id;
    // 是否将ADC数据写入uart1
    uint8_t write_adc_to_uart1;
    // ADC配置
    adc_config_t adc_config;
} system_settings_t;

////////////////////////////////////////////////////////////按地址块区分不同数据——Hold寄存器////////////////////////////////////////////////////////
#define MB_HOLD_RUN_CMD_REG_ADDR 0x0000
#define MB_HOLD_RUN_CMD_REG_NUM sizeof(run_cmd_t) / 2

#define MB_HOLD_HOMOTOR_CMD_REG_ADDR 0x0100
#define MB_HOLD_HOMOTOR_CMD_REG_NUM (sizeof(homotor_cmds_t) / 2)

#define MB_HOLD_LAMOTOR_CMD_REG_ADDR 0x0200
#define MB_HOLD_LAMOTOR_CMD_REG_NUM (sizeof(lamotor_cmds_t) / 2)

#define MB_HOLD_SYS_SETTINGS_REG_ADDR 0x1000
#define MB_HOLD_SYS_SETTINGS_REG_NUM sizeof(system_settings_t) / 2

////////////////////////////////////////////////////////////按地址块区分不同数据——Input寄存器////////////////////////////////////////////////////////
#define MB_INPUT_STATUS_REG_ADDR 0x0000
#define MB_INPUT_STATUS_REG_NUM sizeof(system_status_t) / 2

#define MB_INPUT_VERSION_REG_ADDR 0x0100
#define MB_INPUT_VERSION_REG_NUM sizeof(version_info_t) / 2

#define MB_INPUT_ADC_INFO_REG_ADDR 0x0200
#define MB_INPUT_ADC_INFO_REG_NUM sizeof(adc_info_t) / 2

#define MB_INPUT_HOMOTOR_INFO_REG_ADDR 0x0300
#define MB_INPUT_HOMOTOR_INFO_REG_NUM (sizeof(homotor_info_t) * HO_MOTOR_NUM / 2)

#define MB_INPUT_LAMOTOR_INFO_REG_ADDR 0x0400
#define MB_INPUT_LAMOTOR_INFO_REG_NUM (sizeof(lamotor_info_t) * LA_MOTOR_NUM / 2)

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(homotor_cmd_t) == 22, "homotor_cmd_t size != 22 (pack(2) failed)");
_Static_assert(sizeof(homotor_cmds_t) == 68, "homotor_cmds_t size != 68");
_Static_assert(sizeof(lamotor_cmd_t) == 4, "lamotor_cmd_t size mismatch");
_Static_assert(sizeof(lamotor_cmds_t) == 14, "lamotor_cmds_t size mismatch");
_Static_assert(sizeof(homotor_info_t) == 10, "homotor_info_t size mismatch");
_Static_assert(sizeof(system_settings_t) == 8, "system_settings_t size mismatch");
#endif

#if defined(_MSC_VER) || !(defined(__GNUC__) || defined(__clang__))
#pragma pack(pop)
#endif

#endif // COMMON_DATADEF_H
