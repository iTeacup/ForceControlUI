//This is a window system application program, if it is applied in linux, this program file is not needed
//这是window系统应用的程序，如果在linux中应用，不需这个程序文件
#include "sriCommSerial.h"

CSRICommSerial::CSRICommSerial()
{



}


CSRICommSerial::~CSRICommSerial()
{
	mParserList.clear();	
}

//Open Serial communication
//打开串口通讯
bool CSRICommSerial::OpenPort(std::string   cPort)
{	
	LPCWSTR  Port;
	Port = stringToLPCWSTR(cPort);
	try
	{
		hCom = CreateFile(
			Port,						  //端口号：将要打开的串口逻辑名
			GENERIC_READ | GENERIC_WRITE, //访问模式：允许读和写
			0,							  //共享模式：指定共享属性，由于串口不能共享，该参数必须置为0,独占方式
			NULL,						  //安全设置：引用安全性属性结构，缺省值为NULL
			OPEN_EXISTING,				  //打开而不是创建，该参数表示设备必须存在,否则创建失败
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, //重叠方式
			NULL //对串口而言该参数必须置为NULL
		);
		if (hCom == INVALID_HANDLE_VALUE)
		{
			printf("打开串口失败!\n");
			return FALSE;
		}
		else
		{
			printf("打开串口成功！\n");
		}
		return TRUE;
	}
	catch(std::exception ex)
	{
		mLastError = ex.what();
		return false;
	}
	return true;
}

//runing system
//运行系统
bool CSRICommSerial::run(int BaudRate)
{
	if (Config_Com(BaudRate) == false)
	{
		return false;
	}

	return true;
}


//Configure serial port parameters and start serial port communication
//配置串口参数，启动串口通讯
bool CSRICommSerial::Config_Com(int BaudRate)
{
	SetupComm(hCom, 8192, 8192); //输入缓冲区和输出缓冲区的大小都是1024

	//设定读超时
	COMMTIMEOUTS TimeOuts; 
	TimeOuts.ReadIntervalTimeout = MAXDWORD;
	TimeOuts.ReadTotalTimeoutMultiplier = 0;
	TimeOuts.ReadTotalTimeoutConstant = 0; //设定写超时
	TimeOuts.WriteTotalTimeoutMultiplier = 500;
	TimeOuts.WriteTotalTimeoutConstant = 2000;
	SetCommTimeouts(hCom, &TimeOuts); //设置超时

	//设置串口配置信息
	DCB dcb;
	GetCommState(hCom, &dcb);
	dcb.BaudRate = BaudRate; //波特率为9600
	dcb.ByteSize = 8; //每个字节有8位
	dcb.Parity = NOPARITY; //无奇偶校验位
	dcb.StopBits = TWOSTOPBITS; //两个停止位
	SetCommState(hCom, &dcb);

	//清空缓冲
	PurgeComm(hCom, PURGE_TXCLEAR | PURGE_RXCLEAR);

	m_ovRead.hEvent = CreateEvent(NULL, false, false, NULL);
	m_ovWrite.hEvent = CreateEvent(NULL, false, false, NULL);
	m_ovWait.hEvent = CreateEvent(NULL, false, false, NULL);

	//SetCommMask设置要监控的事件 
	//EV_RXCHAR：输入缓冲区中已收到数据，即接收到一个字节并放入输入缓冲区。
	//EV_ERR：线路状态错误，包括了CE_FRAME / CE_OVERRUN / CE_RXPARITY 3种错误。
	SetCommMask(hCom, EV_ERR | EV_RXCHAR);

	//_beginThreadex创建读取线程  
	m_Thread = (HANDLE)_beginthreadex(NULL, 0, &CSRICommSerial::ComRecv, this, 0, NULL);
	m_IsOpen = true;

	return TRUE;
}
//Serial port sending data
//串口发送数据
bool CSRICommSerial::ComWrite(LPBYTE buf, int& len)
{
	BOOL rtn = FALSE;
	DWORD WriteSize = 0;   //DWORD 代表 unsigned long
	PurgeComm(hCom, PURGE_TXCLEAR | PURGE_TXABORT);
	m_ovWait.Offset = 0;
	rtn = WriteFile(hCom, buf, len, &WriteSize, &m_ovWrite);
	if (FALSE == rtn && WSAGetLastError() == ERROR_IO_PENDING)//后台读取
	{
		//等待数据写入完成
		printf("已发送 ：");
		for (int i = 0; i < len; i++)
			printf("%c ", buf[i]);
		printf("\n");
	}
	return rtn != FALSE;
}

//Serial port to receive data
//串口接收数据
unsigned int __stdcall CSRICommSerial::ComRecv(void* LPParam)//unsigned int __stdcall CSRICommSerial::ComRecv(void* LPParam)
{
	CSRICommSerial* obj = static_cast<CSRICommSerial*>(LPParam);
	DWORD WaitEvent = 0, Bytes = 0;
	BOOL Status = FALSE;
	BYTE ReadBuf[4096];
	int dataBufferLen = 4096;
	int recvDataLen = 0;


	DWORD Error;
	COMSTAT cs = { 0 };
	//int i;

	while (obj->m_IsOpen)
	{
		WaitEvent = 0;
		obj->m_ovWait.Offset = 0;
		Status = WaitCommEvent(obj->hCom, &WaitEvent, &obj->m_ovWait);
		/*
		WaitCommEvent等待串口通信事件的发生
		用途：用来判断用SetCommMask()函数设置的串口通信事件是否已发生。
		原型：BOOL WaitCommEvent(HANDLE hFile,LPDWORD lpEvtMask,LPOVERLAPPED lpOverlapped);
		参数说明：
		-hFile：串口句柄
		-lpEvtMask:函数执行完后如果检测到串口通信事件的话就将其写入该参数中。
		-lpOverlapped：异步结构，用来保存异步操作结果。
		*/

		//GetLastError()函数返回ERROR_IO_PENDING,表明串口正在进行读操作

		if (FALSE == Status && WSAGetLastError() == ERROR_IO_PENDING)
		{
			// GetOverlappedResult函数的最后一个参数设为TRUE，函数会一直等待，直到读操作完成或由于错误而返回。
			Status = GetOverlappedResult(obj->hCom, &obj->m_ovWait, &Bytes, TRUE);
		}
		//在使用ReadFile 函数进行读操作前，应先使用ClearCommError函数清除错误。
		ClearCommError(obj->hCom, &Error, &cs);
		if (TRUE == Status //等待事件成功
			&& WaitEvent & EV_RXCHAR//缓存中有数据到达
			&& cs.cbInQue > 0)//有数据		
		{
			Bytes = 0;
			obj->m_ovRead.Offset = 0;
			/*
			BOOL ReadFile(
			HANDLE hFile, //串口的句柄
			LPVOID lpBuffer,// 读入的数据存储的地址,即读入的数据将存储在以该指针的值为首地址的一片内存区
			DWORD nNumberOfBytesToRead,// 要读入的数据的字节数
			LPDWORD lpNumberOfBytesRead,// 指向一个DWORD数值，该数值返回读操作实际读入的字节数
			LPOVERLAPPED lpOverlapped // 重叠操作时，该参数指向一个OVERLAPPED结构，同步操作时，该参数为NULL
			);
			*/
			memset(ReadBuf, 0, sizeof(ReadBuf));
			Status = ReadFile(obj->hCom, &ReadBuf, dataBufferLen, &Bytes, &obj->m_ovRead);

			//if (Status != FALSE)
			//{
			//	printf("收到 ：");
			//	for (recvDataLen = 0; recvDataLen < Bytes; recvDataLen++)
			//	{
			//		printf("%d ", ReadBuf[recvDataLen]);
			//	}
			//	printf("\n");
			//}
			//PurgeComm函数清空串口的输入输出缓冲区
			PurgeComm(obj->hCom, PURGE_RXCLEAR | PURGE_RXABORT);

			if (Status != FALSE)
			{			
				if ((Bytes > 0) && (Bytes < dataBufferLen))//
				{
					obj->OnReceivedData(ReadBuf, Bytes);
					memset(ReadBuf, 0, Bytes);
				}
				else if (Bytes == 0)
				{
					continue;
				}
			}

		}
	}
	return 0;
}
//On Received Data
//接收数据
bool CSRICommSerial::OnReceivedData(BYTE* data, int dataLen)
{
	for (size_t i = 0; i < mParserList.size(); ++i)
	{
		CSRICommParser* parser = mParserList[i];
		if (parser != NULL)
		{
			parser->OnReceivedData(data, dataLen);
		}
	}
	return true;
}

//Close Port
//关闭串口
void CSRICommSerial::Close_Com()
{
	m_IsOpen = false;
	if (INVALID_HANDLE_VALUE != hCom)
	{
		CloseHandle(hCom);
		hCom = INVALID_HANDLE_VALUE;
	}
	if (NULL != m_ovRead.hEvent)
	{
		CloseHandle(m_ovRead.hEvent);
		m_ovRead.hEvent = NULL;
	}
	if (NULL != m_ovWrite.hEvent)
	{
		CloseHandle(m_ovWrite.hEvent);
		m_ovWrite.hEvent = NULL;
	}
	if (NULL != m_ovWait.hEvent)
	{
		CloseHandle(m_ovWait.hEvent);
		m_ovWait.hEvent = NULL;
	}
	if (NULL != m_Thread)
	{
		WaitForSingleObject(m_Thread, 5000);//等待线程结束  
		CloseHandle(m_Thread);
		m_Thread = NULL;
	}
}






//Check if the connection timed out
//检测连接超时
bool CSRICommSerial::CheckTimeoutError()
{
#ifdef  IS_WINDOWS_OS
	int errorCode = WSAGetLastError();
	if (WSAETIMEDOUT == errorCode)//
	{
		return true;
	}
#else
	//#define ETIMEDOUT       110     /* Connection timed out */  
	if (ETIMEDOUT == errno)
	{
		return true;
	}
#endif
	return false;
}

//Get error information
//获取错误信息
void CSRICommSerial::GetLastSerialError(std::string functionName ="")
{
	mLastError = "";
#ifdef  IS_WINDOWS_OS
	char buffer[2048];
	memset(buffer, 0, 2048);
	sprintf(buffer, "Serial %s error: %d\n", functionName.data(), WSAGetLastError());
	mLastError = buffer;
#else
	char buffer[2048];
	memset(buffer, 0, 2048);
	sprintf(buffer, "Serial %s error: %s(errno: %d)\n", functionName.data(), strerror(errno), errno);
	mLastError = buffer;
#endif

	//OnNetworkFailure();
}


//Add parser
//添加解析器
bool CSRICommSerial::AddCommParser(CSRICommParser* parser)
{
	mParserList.push_back(parser);
	return true;
}

bool CSRICommSerial::SetNetworkFailureCallbackFunction(SRICommNetworkFailureCallbackFunction networkFailureCallback)
{
	mNetworkFailureCallback = networkFailureCallback;
	return true;
}

std::string CSRICommSerial::GetLastError()
{
	return mLastError;
}

LPCWSTR stringToLPCWSTR(std::string orig)
{
	size_t origsize = orig.length() + 1;
	const size_t newsize = 100;
	size_t convertedChars = 0;
	wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (orig.length() - 1));
	mbstowcs_s(&convertedChars, wcstring, origsize, orig.c_str(), _TRUNCATE);
	return wcstring;
}