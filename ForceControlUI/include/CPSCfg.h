#pragma once
#include "ModbusDef.h"

struct ST_MBCfg
{
    EMBBaudRate baud_rate; // Baud rate
    int slave_id;          // Slave ID
    int com_index;         // Communication port index
};

struct ST_SerialCfg
{
    EMBBaudRate baud_rate; // Baud rate
    int com_index;         // Communication port index
};

typedef struct
{
    char ip[16];
    int port;
} ST_BusCfg;

typedef struct
{
    char ip[16];
    int port;
} ST_LogCfg;

typedef struct
{
    ST_BusCfg bus;
    ST_LogCfg log;
} ST_BusLogCfg;

typedef struct
{
    char server_ip[16];
    char local_ip[16];
} ST_OptServerCfg;

typedef struct
{
    char com_port[32];
    int baud_rate;
    int sample_rate; // Sampling rate
} ST_SRIServerCfg;
