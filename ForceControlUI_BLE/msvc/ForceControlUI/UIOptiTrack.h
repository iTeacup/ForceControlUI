#pragma once
#include "UIBaseWindow.h"
#include "CPSOptServerDef.h"
#include <mutex>
#include <vector>
#include <map>
#include <string>

class UIOptiTrack :
    public UIBaseWindow
{
public:
	UIOptiTrack(UIMainWindowBase* main_win, const char* title);
	~UIOptiTrack();

	virtual void Draw();

	virtual EUIMenuCategory GetWinMenuCategory() { return EUIMenuCategory::E_UI_CAT_APP; }
	virtual const char* GetShowShortCut() { return "Ctrl+6"; }

	virtual void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);

	void GetSelectedMarkerData(ST_OptMarker_List& data);

	void SaveSelectedMarkers(const std::string& header_info = "");
	void SaveAllMarkers();
protected:
	void OnInitRsp(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);
	void OnStopRsp(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);
	void OnMarkerListMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len);
	void SaveVecData();
	void SaveVecMarker(int id, const std::vector<ST_OptMarker> & vec);

	void ResetCache();

	int GetIndexByID(int ID);
	void ResetMarkerInfo();
	void InitOptService();
	void StopOptService();
protected:
	std::mutex m_marker_list_lock;
	ST_OptMarker_List m_marker_list = { 0 };

	int m_disp_marker_count = 6;
	//std::vector<int> m_selected_index;
	std::vector<int> m_selected_ids;

	std::mutex m_vec_data_lock;
	std::map<int, std::vector<ST_OptMarker>> m_vec_data;

	std::atomic_bool m_start_serialize = false;
	size_t m_req_id = 0;
};

