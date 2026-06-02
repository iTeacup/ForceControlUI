#include <stdio.h>
#include <iostream>
#include <thread>
#include <map>
#include <atomic>

#include "NatNet/NatNetTypes.h"
#include "NatNet/NatNetCAPI.h"
#include "NatNet/NatNetClient.h"
#include "CPSHandler.h"
#include "MarkerInfo.h"
#include "OptCfgParser.h"

/////////////////////////////////////////////////////////////////////////////////////
std::atomic_bool g_start_rec = false;
std::atomic_bool g_exit = false;

void ThreadFunc(MarkerInfo* marker_info)
{
	while (!g_exit)
	{
		if (!g_start_rec)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		// send marker info
		if (marker_info)
		{
			marker_info->PushMarkerInfo();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(int(1000.0 / g_cfg->m_push_freq_hz)));
	}
}

void NATNET_CALLCONV DataHandler(sFrameOfMocapData* data, void* pUserData);    // receives data from the server
void NATNET_CALLCONV MessageHandler(Verbosity msgType, const char* msg);      // receives NatNet error messages

int main()
{
	static const ConnectionType kDefaultConnectionType = ConnectionType_Multicast;

	// load cfg
	if (!g_cfg->LoadCfg())
	{
		CPS_ERROR("¼ÓÔØÅäÖÃÎÄ¼þÊ§°Ü£¡");
		return -1;
	}

	NatNetClient* client = NULL;
	sNatNetClientConnectParams params;
	sServerDescription server_desc;

	CCPSAPI* cps_api = CCPSAPI::CreateAPI();

	// create marker info
	MarkerInfo* marker_info = new MarkerInfo(cps_api);

	Handler handler(cps_api, &g_start_rec, marker_info);
	if (cps_api->Init(E_CPS_TYPE_DEVICE, g_cfg->m_dev_id , 0,
		g_cfg->m_cpscfg.bus.ip, g_cfg->m_cpscfg.bus.port, 
		g_cfg->m_cpscfg.log.ip, g_cfg->m_cpscfg.log.port, &handler) != 0)
	{
		CPS_ERROR("API init failed.");
		return -1;
	}
	
	// print version info
	unsigned char ver[4];
	NatNet_GetVersion(ver);
	CPS_INFO("NatNet Sample Client (NatNet ver. %d.%d.%d.%d).", ver[0], ver[1], ver[2], ver[3]);

	// Install logging callback
	NatNet_SetLogCallback(MessageHandler);

	// create NatNet client
	client = new NatNetClient();

	// set the frame callback handler
	client->SetFrameReceivedCallback(DataHandler, marker_info);	// this function will receive data from the server

	params.connectionType = kDefaultConnectionType;
	params.serverAddress = g_cfg->m_opt_cfg.server_ip;
	params.localAddress = g_cfg->m_opt_cfg.local_ip;

	auto exit_func = [&]() {
		// Done - clean up.
		if (client)
		{
			client->Disconnect();
			delete client;
			client = nullptr;
		}
		if (marker_info)
		{
			delete marker_info;
			marker_info = nullptr;
		}
		cps_api->Release();
		exit(0);
	};

	// connect server
	client->Disconnect();
	int retCode = client->Connect(params);
	if (retCode != ErrorCode_OK)
	{
		CPS_ERROR("Unable to connect to optitrack server.  Error code: %d. Exiting...", retCode);
		exit_func();
	}
	ErrorCode ret = ErrorCode_OK;
	// print server info
	memset(&server_desc, 0, sizeof(server_desc));
	ret = client->GetServerDescription(&server_desc);
	if (ret != ErrorCode_OK || !server_desc.HostPresent)
	{
		CPS_ERROR("Unable to connect to optitrack server. Host not present. Exiting...");
		exit_func();
	}
	// get mocap frame rate	
	void* pResult;
	int nBytes = 0;
	//sDataDescriptions
	ret = client->SendMessageAndWait("FrameRate", &pResult, &nBytes);
	if (ret == ErrorCode_OK)
	{
		float fRate = *((float*)pResult);
		CPS_INFO("Mocap Framerate : %3.2f", fRate);
	}

	std::thread timer_thread(ThreadFunc, marker_info);
	
	printf("enter q to exit: \n");

	while (true)
	{
		char q = 0;
		std::cin >> q;

		if (q == 'q')
		{
			break;
		}
	}
	// exit
	g_exit = true;
	timer_thread.join();
	cps_api->Release();

	return ErrorCode_OK;
}

void NATNET_CALLCONV DataHandler(sFrameOfMocapData* data, void* pUserData)
{
	MarkerInfo* marker_info = (MarkerInfo*)pUserData;

	// labeled markers - this includes all markers (Active, Passive, and 'unlabeled' (markers with no asset but a PointCloud ID)
	bool bOccluded;     // marker was not visible (occluded) in this frame
	bool bPCSolved;     // reported position provided by point cloud solve
	bool bModelSolved;  // reported position provided by model solve
	bool bHasModel;     // marker has an associated asset in the data stream
	bool bUnlabeled;    // marker is 'unlabeled', but has a point cloud ID that matches Motive PointCloud ID (In Motive 3D View)
	bool bActiveMarker; // marker is an actively labeled LED marker

	for (int i = 0; i < data->nLabeledMarkers; i++)
	{
		bOccluded = ((data->LabeledMarkers[i].params & 0x01) != 0);
		bPCSolved = ((data->LabeledMarkers[i].params & 0x02) != 0);
		bModelSolved = ((data->LabeledMarkers[i].params & 0x04) != 0);
		bHasModel = ((data->LabeledMarkers[i].params & 0x08) != 0);
		bUnlabeled = ((data->LabeledMarkers[i].params & 0x10) != 0);
		bActiveMarker = ((data->LabeledMarkers[i].params & 0x20) != 0);

		sMarker marker = data->LabeledMarkers[i];

		// Marker ID Scheme:
		// Active Markers:
		//   ID = ActiveID, correlates to RB ActiveLabels list
		// Passive Markers: 
		//   If Asset with Legacy Labels
		//      AssetID 	(Hi Word)
		//      MemberID	(Lo Word)
		//   Else
		//      PointCloud ID

		// For active markers, this is the Active ID. For passive markers, this is the PointCloud assigned ID.
		// For legacy assets that are created prior to 2.0, this is both AssetID (High-bit) and Member ID (Lo-bit)
		
		//int modelID, markerID;
		//NatNet_DecodeID(marker.ID, &modelID, &markerID);
		
		if(marker_info)
		{
			marker_info->UpdateMarker(marker.ID, marker.x * 1000, marker.y * 1000, marker.z * 1000);
		}
		//printf("%s Marker [ModelID=%d, MarkerID=%d, Occluded=%d, PCSolved=%d, ModelSolved=%d] [size=%3.2f] [pos=%3.2f,%3.2f,%3.2f]\n",
		//	szMarkerType, modelID, markerID, bOccluded, bPCSolved, bModelSolved, marker.size, marker.x, marker.y, marker.z);
	}
}

void NATNET_CALLCONV MessageHandler(Verbosity msgType, const char* msg)
{
	// Optional: Filter out debug messages
	if (msgType < Verbosity_Info)
	{
		return;
	}

	printf("\n[NatNetLib]");

	switch (msgType)
	{
	case Verbosity_Debug:
		printf(" [DEBUG]");
		break;
	case Verbosity_Info:
		printf("  [INFO]");
		break;
	case Verbosity_Warning:
		printf("  [WARN]");
		break;
	case Verbosity_Error:
		printf(" [ERROR]");
		break;
	default:
		printf(" [?????]");
		break;
	}

	printf(": %s\n", msg);
}

