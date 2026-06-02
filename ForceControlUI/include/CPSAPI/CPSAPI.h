#pragma once

#if defined _WIN32
#   if defined CPS_API_STATIC
#       define CPS_API_EXPORT
#   elif defined CPS_API_DLL
#       define CPS_API_EXPORT __declspec(dllexport)
#   else
#       define CPS_API_EXPORT __declspec(dllimport)
#   endif
#else
#   define CPS_API_EXPORT
#endif

#include <stdint.h>
#include <string.h>


// CPSAPI type
enum E_CPS_TYPE
{
	E_CPS_TYPE_DEVICE = 0x1,
	E_CPS_TYPE_APP = 0x2,
	E_CPS_TYPE_MIX = E_CPS_TYPE_DEVICE | E_CPS_TYPE_APP
};


class CCPSEventHandler
{
public: // interfaces
	// Connect event
	virtual void OnConnected() {};
	// Disconnect event
	virtual void OnDisconnected() {};
	// Device Online event
	virtual void OnDeviceOnline(uint32_t dev_id) {};
	// Device Offline event
	virtual void OnDeviceOffline(uint32_t dev_id) {};
	// Message event
	virtual void OnMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len) {};

	virtual ~CCPSEventHandler() {};
};

/*
	Note:
	1. API Users are classcified into two types, Device and APP
	2. Device should not use methods designed for APP, vice versa.
	3. Device can send msg to specified APP which subscribes this device
	4. Device can broadcast msg to all apps which subscribe this device
	5. Device can not send msg to another device
	6. APP can send msg to specified device which itself subscribes
	7. APP can not send msg to APP, if this is necessary, use mixed types
	About mixed types:
	1. If the preceding rules do not meet your needs, this api can also support mixed type
	2. Mixed type should call both functions: RegisterDevice & RegisterAPP 
	3. At this time, all the rules can be used for mixed type
*/
class CPS_API_EXPORT CCPSAPI
{
public:
	// Create API Instance
	static CCPSAPI* CreateAPI();
	// Get API Version Info
	static const char* GetApiVersion();

	// Api init
	// for E_CPS_TYPE_DEVICE, dev_id is used
	// for E_CPS_TYPE_APP, app_id is used
	// for E_CPS_TYPE_MIX, both dev_id and app_id are used
	// Note: device id is not arbitray, please ask your admin for your device id
	// APP id should be in range [0x1001, 0x2000], and should be unique in the whole system
	virtual int Init(E_CPS_TYPE api_type, uint32_t dev_id, uint32_t app_id,
		const char* server_ip, int server_port,
		const char* log_ip, int log_port,
		CCPSEventHandler* handler) = 0;

	// Release api
	virtual void Release() = 0;

	///////////////////////////////////////////////////////////////////////
	// These methods are used by device, should be called after connection established
	// Register Device. 
	virtual int RegisterDevice() = 0;
	// UnRegister Device
	virtual int UnregisterDevice() = 0;
	// Send Message as Device. If app_id is -1, send becomes broadcast
	virtual int SendDeviceMsg(int app_id, uint32_t msg_type, const char* data, uint32_t msg_len) = 0;
	///////////////////////////////////////////////////////////////////////
	// These methods are used by APP, should be called after connection established
	// Subscribe Device
	virtual int SubscribeDevice(uint32_t device_id) = 0;
	// Unsubscribe Device
	virtual int UnSubscribeDevice(uint32_t device_id) = 0;
	// Test device online or not
	// Note: subscribe the device before using this method
	virtual bool IsDeviceOnline(uint32_t device_id) = 0;
	// Send Message as APP
	virtual int SendAPPMsg(uint32_t device_id, uint32_t msg_type, const char* data, uint32_t msg_len) = 0;

	// Send Server Message, reserved interface
	virtual int SendMsg(uint32_t msg_type, const char* data, uint32_t msg_len) = 0;

	virtual ~CCPSAPI() {}
};

//////////////////////////////////////////////////////////////////////////
/* Log utility */
#ifdef _DEBUG
#define __FILENAME__	__FILE__
#else
#ifdef _WIN32
#define __FILENAME__ (strrchr("\\" __FILE__, '\\') + 1)
#else
#define __FILENAME__ (strrchr("/" __FILE__, '/') + 1)
#endif
#endif

//////////////////////////////////////////////////////////////////////////
/* Write log api
	return 1 for success, 0 for failure
*/
CPS_API_EXPORT int cps_log(unsigned int level, const char* format, ...);

//////////////////////////////////////////////////////////////////////////
#define CPS_LOG_FORMAT_PREFIX "(File:%s Line:%d)"
// useful macros
#define CPS_DEBUG(fmt,...)				cps_log(100, CPS_LOG_FORMAT_PREFIX fmt, __FILENAME__, __LINE__, ##__VA_ARGS__)
#define CPS_INFO(fmt,...)				cps_log(200,  CPS_LOG_FORMAT_PREFIX fmt, __FILENAME__, __LINE__, ##__VA_ARGS__)
#define CPS_WARN(fmt,...)				cps_log(300,  CPS_LOG_FORMAT_PREFIX fmt, __FILENAME__, __LINE__, ##__VA_ARGS__)
#define CPS_ERROR(fmt,...)				cps_log(400, CPS_LOG_FORMAT_PREFIX fmt, __FILENAME__, __LINE__, ##__VA_ARGS__)
#define CPS_FATAL(fmt,...)				cps_log(500, CPS_LOG_FORMAT_PREFIX fmt, __FILENAME__, __LINE__, ##__VA_ARGS__)
