#pragma once
#include <CPSAPI/CPSAPI.h>
#include "CPSOptServerDef.h"

#include <mutex>
#include <unordered_map>

class MarkerInfo
{
public:
	MarkerInfo(CCPSAPI* api):m_api(api)
	{

	}

	void UpdateValidMarkerIDList(uint32_t from_id, ST_ValidMarkerIDList * ids)
	{
		CPS_INFO("UpdateValidMarkerIDList...");
		memcpy(&m_marker_id_list, ids, sizeof(ST_ValidMarkerIDList));
	}

	void UpdateMarker(int id, float x, float y, float z)
	{
		// 查找ID是否有效
		auto result = std::find(std::begin(m_marker_id_list.id_list), std::end(m_marker_id_list.id_list), id);
		if (m_marker_id_list.id_num > 0 &&  // 如果没有指定有效的marker id list, 则默认采集所有marker id
			result == std::end(m_marker_id_list.id_list)) 
			return;

		std::lock_guard<std::mutex> lock(m_markers_lock);
		m_markers[id].ID = id;
		m_markers[id].XYZ[0] = x;
		m_markers[id].XYZ[1] = y;
		m_markers[id].XYZ[2] = z;
	}

	void ResetMarkers()
	{
		std::lock_guard<std::mutex> lock(m_markers_lock);
		m_markers.clear();
	}

	void PushMarkerInfo()
	{
		ST_OptMarker_List marker_list = { 0 };
		{
			std::lock_guard<std::mutex> lock(m_markers_lock);
			for (const auto& iter : m_markers)
			{
				marker_list.markers[marker_list.marker_num++] = iter.second;
				// 最多发送MAX_MARKER_NUM个数据
				if (marker_list.marker_num >= MAX_MARKER_NUM - 1)
				{
					break;
				}
			}
		}
		m_api->SendDeviceMsg(-1, MSG_OPTITRACK_MARKER_INFO,	(const char*)&marker_list, sizeof(ST_OptMarker_List));
	}

protected:
	CCPSAPI* m_api;
	std::mutex m_markers_lock;
	std::unordered_map<int, ST_OptMarker> m_markers;
	ST_ValidMarkerIDList m_marker_id_list = { 0 };
};
