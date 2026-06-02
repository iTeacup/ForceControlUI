#pragma once
#include <stdint.h>

enum EMBConnType
{
	E_RTU = 0,
	E_TCP
};

enum EMBBaudRate
{
	E_9600 = 0,
	E_14400,
	E_19200,
	E_38400,
	E_56000,
	E_57600,
	E_115200,
    E_230400,
    E_250000,
    E_500000,
    E_921600,
    E_1000000,
    E_2000000,
    E_3000000,
    E_4000000
};

inline int GetBaudRate(EMBBaudRate baudrate)
{
    switch (baudrate)
    {
    case E_9600: return 9600;
    case E_14400: return 14400;
    case E_19200: return 19200;
    case E_38400: return 38400;
    case E_56000: return 56000;
    case E_57600: return 57600;
    case E_115200: return 115200;
    case E_230400: return 230400;
    case E_250000: return 250000;
    case E_500000: return 500000;
    case E_921600: return 921600;
    case E_1000000: return 1000000;
    case E_2000000: return 2000000;
    case E_3000000: return 3000000;
    case E_4000000: return 4000000;
    default: return 9600; // return default baud rate if not recognized
    }
}

inline EMBBaudRate GetBaudRateEnum(int baudrate)
{
    switch (baudrate)
    {
    case 9600: return E_9600;
    case 14400: return E_14400;
    case 19200: return E_19200;
    case 38400: return E_38400;
    case 56000: return E_56000;
    case 57600: return E_57600;
    case 115200: return E_115200;
    case 230400: return E_230400;
    case 250000: return E_250000;
    case 500000: return E_500000;
    case 921600: return E_921600;
    case 1000000: return E_1000000;
    case 2000000: return E_2000000;
    case 3000000: return E_3000000;
    case 4000000: return E_4000000;
    default: return E_9600; // return default baud rate if not recognized
    }
}