#include "sriCommATParser.h"



CSRICommATParser::CSRICommATParser()
{
	mCircularBuffer.Init(102400);//100KB
	mAtCallbackFunction = NULL;
}


CSRICommATParser::~CSRICommATParser()
{
}
//AT command response processing function
//AT指令应答处理函数
bool CSRICommATParser::SetATCallbackFunction(SRICommATCallbackFunction atCallbackFunction)
{
	mAtCallbackFunction = atCallbackFunction;
	return false;
}
//Receive data processing
//接收数据处理
bool CSRICommATParser::OnReceivedData(BYTE* data, int dataLen)
{
	if (data == NULL)
	{
		return false;
	}
	if (dataLen <= 0)
	{
		return false;
	}
	mCircularBuffer.Write(data, dataLen);
	//
	int delLen = 0;
	std::string ack = "";
	if (ParseDataFromBuffer(delLen, ack) == false)
	{
		mCircularBuffer.Clear(delLen);
		return false;
	}
	mCircularBuffer.Clear(delLen);
	
	if (mAtCallbackFunction != NULL)
	{
		mAtCallbackFunction(ack);
	}
	return true;
}

bool CSRICommATParser::OnNetworkFailure(std::string infor)
{
	return true;
}


//Parser ACK command
//解析ACK指令
bool CSRICommATParser::ParseDataFromBuffer(int& delLen, std::string& ack)
{
	int dataLen = 0;
	BYTE* data = mCircularBuffer.ReadTry(dataLen);//读取缓存数据//Read circular buffer data
	if (data == NULL)
	{
		return false;
	}
	// ACK +
	//Data length is too small
	//数据长度太小
	if (dataLen < 4)
	{
		delLen = 0;
		ack = "";
		return false;
	}
	//Get ACK command head index
	//获取帧头位置
	int headIndex = ParseGetHeadIndex(data, dataLen);
	if (headIndex == -1)
	{
		delLen = dataLen - 3; //找不到帧头，删除当前所有数据，保留3/4个帧头。
		ack = "";
		return false;
	}

	//Get the end index of the ACK command
	//获取帧尾位置
	int endIndex = ParseGetEndIndex(data, dataLen, headIndex + 4);
	if (endIndex == -1)
	{
		delLen = headIndex;
		ack = "";
		return false;
	}
	//
	int len = endIndex - headIndex + 2;
	char* command = new char[len];
	memcpy(command, data + headIndex, len);
	std::string commandStr = command;//应答指令内容
	delete command;
	ack = commandStr;//应答指令内容
	delLen = endIndex + 2;
	return true;
}
//Get ACK command head index
//获取ACK指令应答帧头位置
int CSRICommATParser::ParseGetHeadIndex(BYTE* data, int dataLen)
{
	int headIndex = -1;
	for (int i = 0; i < dataLen - 3; i++)
	{
		// "ACK+"
		if ((data[i] == 65) && (data[i + 1] == 67) && (data[i + 2] == 75) && (data[i + 3] == 43))
		{
			headIndex = i;
			break;
		}
	}
	return headIndex;
}

//Get the end index of the ACK command
//获取AT指令应答帧尾数据
int CSRICommATParser::ParseGetEndIndex(BYTE* data, int dataLen, int index)
{
	int endIndex = -1;
	for (int i = index; i < dataLen - 1; i++)
	{
		if ((data[i] == 13) && (data[i + 1] == 10))
		{
			endIndex = i;
			break;
		}
	}
	return endIndex;
}