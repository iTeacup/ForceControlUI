#pragma once

enum class E_UI_LOG_LEVEL {
	E_UI_LOG_DEBUG = 0,
	E_UI_LOG_INFO,
	E_UI_LOG_WARN,
	E_UI_LOG_ERROR,
	E_UI_LOG_FATAL
};

inline const char* LogLevel2Str(E_UI_LOG_LEVEL level)
{
	switch (level)
	{
	case E_UI_LOG_LEVEL::E_UI_LOG_DEBUG:
		return "Debug";
	case E_UI_LOG_LEVEL::E_UI_LOG_INFO:
		return "Info";
	case E_UI_LOG_LEVEL::E_UI_LOG_WARN:
		return "Warn";
	case E_UI_LOG_LEVEL::E_UI_LOG_ERROR:
		return "Error";
	case E_UI_LOG_LEVEL::E_UI_LOG_FATAL:
		return "Fatal";
	default:
		return "Info";
	}
}
