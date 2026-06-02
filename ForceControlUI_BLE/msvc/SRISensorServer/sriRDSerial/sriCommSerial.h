#ifndef SRI_COMM_SERIAL_H
#define SRI_COMM_SERIAL_H

#include "sriCommDefine.h"
#include "sriCommParser.h"



class CSRICommSerial
{
public:
	CSRICommSerial();
	~CSRICommSerial();
	bool OnReceivedData(BYTE* data, int dataLen);
	bool OpenPort(std::string   Port);
	bool Config_Com(int BaudRate);//配置串口
	bool ComWrite(LPBYTE buf, int& len);
	unsigned static int __stdcall ComRecv(void*);//读取数据//
	void Close_Com();
	bool AddCommParser(CSRICommParser* parser);
	std::string GetLastError();

	bool run(int BaudRate);

	bool SetNetworkFailureCallbackFunction(SRICommNetworkFailureCallbackFunction networkFailureCallback);

private:
	HANDLE hCom;
	OVERLAPPED m_ovWrite;//用于写入数据
	OVERLAPPED m_ovRead;//用于读取数据
	OVERLAPPED m_ovWait;//用于等待数据
	volatile bool m_IsOpen;//串口是否打开
	HANDLE m_Thread;//读取线程句柄



	struct sockaddr_in mLocalAddr;
	struct sockaddr_in mRemoteAddr;
	bool CheckTimeoutError();
	std::thread mThread;
	bool mIsStopThread;
	bool mIsTreadStoped;

	void GetLastSerialError(std::string functionName);

	std::vector<CSRICommParser*> mParserList;

	SRICommNetworkFailureCallbackFunction mNetworkFailureCallback;

	std::string mLastError;

};

#endif

LPCWSTR stringToLPCWSTR(std::string orig);