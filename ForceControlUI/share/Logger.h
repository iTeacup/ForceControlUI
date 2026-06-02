#pragma once
#include "Config.h"

#if defined UI_CFG_LOG_TO_CONSOLE
#define LOG_DEBUG(fmt, ...)		printf(fmt "\n", __VA_ARGS__)
#define LOG_INFO(fmt, ...)		printf(fmt "\n", __VA_ARGS__)
#define LOG_WARN(fmt, ...)		printf(fmt "\n", __VA_ARGS__)
#define LOG_ERROR(fmt, ...)		printf(fmt "\n", __VA_ARGS__)
#define LOG_FATAL(fmt, ...)		printf(fmt "\n", __VA_ARGS__)
#elif defined UI_CFG_LOG_TO_CPS
#include <CPSAPI/CPSAPI.h>
#define LOG_DEBUG		CPS_DEBUG
#define LOG_INFO		CPS_INFO
#define LOG_WARN		CPS_WARN
#define LOG_ERROR		CPS_ERROR
#define LOG_FATAL		CPS_FATAL
#else
#include "UILog.h"
#define LOG_DEBUG		UI_DEBUG
#define LOG_INFO		UI_INFO 
#define LOG_WARN		UI_WARN
#define LOG_ERROR		UI_ERROR
#define LOG_FATAL		UI_FATAL
#endif