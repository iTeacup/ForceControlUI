#include "sriCommM8218Parser.h"



CSRICommM8218Parser::CSRICommM8218Parser()
{
	mCircularBuffer.Init(102400);//100KB
	mM8218CallbackFunction = NULL;
}


CSRICommM8218Parser::~CSRICommM8218Parser()
{
}


//显示解析完成后的数据
//Display the data got from M8128
bool CSRICommM8218Parser::SetM8218CallbackFunction(SRICommM8218CallbackFunction m8218CallbackFunction)
{
	mM8218CallbackFunction = m8218CallbackFunction;
	return true;
}

//m8128 data processing
//m8128数据处理
bool CSRICommM8218Parser::OnReceivedData(BYTE* data, int dataLen)
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
	float fx;
	float fy;
	float fz;
	float mx;
	float my;
	float mz;
	if (ParseDataFromBuffer(delLen, fx, fy, fz, mx, my, mz) == false)
	{
		mCircularBuffer.Clear(delLen);
		return false;
	}	
	//Clear circular buffer data that has been processed
	//清除已经处理的缓存数据
	mCircularBuffer.Clear(delLen);	
	//
	if (mM8218CallbackFunction != NULL)
	{
		mM8218CallbackFunction(fx, fy, fz, mx, my, mz);
	}
	return true;
}

bool CSRICommM8218Parser::OnNetworkFailure(std::string infor)
{
	return true;
}

//Parse m8128 data
//m8128数据解析
bool CSRICommM8218Parser::ParseDataFromBuffer(int& delLen, float& fx, float& fy, float& fz, float& mx, float& my, float& mz)
{
	delLen = 0;
	int dataLen = 0;
	BYTE* data = mCircularBuffer.ReadTry(dataLen);
	if (data == NULL)
	{
		return false;
	}
	//
	int headIndex = ParseGetHeadIndex(data, dataLen);
	if (headIndex == -1)
	{
		delLen = dataLen - 1; 
		return false;
	}
	//M8128 data length 31 bits (checksum data mode)
	//M8128数据长度31位（校验和数据模式)
	if (headIndex + 31 > dataLen)
	{
		//
		delLen = headIndex; 
		return false;
	}
	//Frame data length 27, calculated from the start bit of the packet number
	//帧数据长度27，从包编号起始位开始计算
	int index = headIndex + 2;
	int frameLen = data[index + 0] * 256 + data[index + 1];
	if (frameLen != 27)
	{
		delLen = index; 
		return false;
	}
	index = index + 2;
	//
	//int frameNo = data[index + 0] * 256 + data[index + 1];
	index = index + 2;
	//
	fx = 0;
	memcpy(&fx, data + index, 4);
	index = index + 4;
	fy = 0;
	memcpy(&fy, data + index, 4);
	index = index + 4;
	fz = 0;
	memcpy(&fz, data + index, 4);
	index = index + 4;

	mx = 0;
	memcpy(&mx, data + index, 4);
	index = index + 4;
	my = 0;
	memcpy(&my, data + index, 4);
	index = index + 4;
	mz = 0;
	memcpy(&mz, data + index, 4);
	index = index + 4;

	//
	BYTE check = data[index];
	index = index + 1;
	//
	delLen = index; 
	//
	BYTE checkNew = 0x00;
	for (int i = headIndex + 6; i <= headIndex + 4 + 24 +1; i++)
	{
		checkNew = (checkNew + data[i]) ;
	}
	if (checkNew != check)
	{
		return false;
	}


	return true;
}

//Get M8128 data head index
//获取M8128数据数据帧头位置
int CSRICommM8218Parser::ParseGetHeadIndex(BYTE* data, int dataLen)
{
	int headIndex = -1;
	int countHead = 0;
	for (int i = 0; i < dataLen - 1; i++)
	{
		if ((data[i] == 0xAA) && (data[i + 1] == 0x55))
		{
			headIndex = i;
			countHead++;
		}
	}

	if (headIndex + 31 > dataLen)
	{		
		if (countHead > 1)
		{
			int RcountHead = 0;
			for (int i = 0; i < dataLen - 1; i++)
			{
				if ((data[i] == 0xAA) && (data[i + 1] == 0x55))
				{
					headIndex = i;
					RcountHead++;
					if (RcountHead == (countHead - 1))
					{
						return headIndex;
					}
				}
			}
		}
	}
	return headIndex;
}