#pragma once
#include <CPSAPI/CPSAPI.h>
#include <CPSAPI/CPSDef.h>
#include "MarkerInfo.h"

class Handler : public CCPSEventHandler
{
public:
	Handler(CCPSAPI* api, std::atomic_bool* start_rec, MarkerInfo* marker_info) :m_api(api), m_start_rec(start_rec), m_marker_info(marker_info)
	{}
	// Connect event
	virtual void OnConnected()
	{
		m_api->RegisterDevice();
		printf("OnConnected\n");
	}
	// Disconnect event
	virtual void OnDisconnected()
	{
		printf("OnDisconnected\n");
	}
	// Message event
	virtual void OnMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len)
	{
		switch (msg_type)
		{
		case MSG_CMD_INIT:
			this->OnInit(from_id, (ST_CMDInit*)data);
			break;
		case MSG_CMD_STOP:
			this->OnStop(from_id, (ST_CMDStop*)data);
			break;
		case MSG_SET_VALID_MARKER_ID:
			m_marker_info->UpdateValidMarkerIDList(from_id, (ST_ValidMarkerIDList*)data);
			break;
		case MSG_RESET_MARKER_ID:
			m_marker_info->ResetMarkers();
			break;
		default:
			break;
		}
	}

	void OnInit(uint32_t from_id, ST_CMDInit* req)
	{
		printf("收到初始化请求.\n");
		ST_CMDInitRsp Rsp = { 0 };
		*m_start_rec = true;
		Rsp.req_no = req->req_no;
		Rsp.rsp.error_code = 0;
		strcat(Rsp.rsp.error_msg, "硬件初始化成功");
		m_api->SendDeviceMsg(from_id, MSG_CMD_INIT_RSP, (const char*)&Rsp, sizeof(ST_CMDInitRsp));
	}

	void OnStop(uint32_t from_id, ST_CMDStop* req)
	{
		printf("收到停止请求.\n");
		ST_CMDStopRsp Rsp = { 0 };
		*m_start_rec = false;
		Rsp.req_no = req->req_no;
		Rsp.rsp.error_code = 0;
		strcat(Rsp.rsp.error_msg, " Stop Success");
		m_api->SendDeviceMsg(from_id, MSG_CMD_STOP_RSP, (const char*)&Rsp, sizeof(ST_CMDStopRsp));
	}
private:
	CCPSAPI* m_api;
	std::atomic_bool * m_start_rec;
	MarkerInfo* m_marker_info;
};
