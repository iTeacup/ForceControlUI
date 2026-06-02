#pragma once
#include <CPSAPI/CPSDef.h>

constexpr const unsigned int MAX_MARKER_NUM = 15;

#pragma pack(push, 4)
////////////////////////////////通用消息定义////////////////////////////////////////
// 设置markerid 
#define MSG_SET_VALID_MARKER_ID						0x601
typedef struct
{
	int id_num;
	int id_list[MAX_MARKER_NUM];
}ST_ValidMarkerIDList;
// 重置所有marker id
#define MSG_RESET_MARKER_ID							0x602

////////////////////////////////Optitrack服务消息////////////////////////////////////////
// Optitrack marker 信息
typedef struct
{
	int ID;
	float XYZ[3];
}ST_OptMarker;

#define	MSG_OPTITRACK_MARKER_INFO					0x610
typedef struct
{
	int marker_num;
	ST_OptMarker markers[MAX_MARKER_NUM];
}ST_OptMarker_List;

#pragma pack(pop)

