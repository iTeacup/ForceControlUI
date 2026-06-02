#include "sriCommManager.h"
#include <string>


CSRICommManager::CSRICommManager()
{
	fx_main = 0;
	fy_main = 0;
	fz_main = 0;
	mx_main = 0;
	my_main = 0;
	mz_main = 0;
}


CSRICommManager::~CSRICommManager()
{
}
//initialization
//初始化
bool CSRICommManager::Init(std::string  com)
{

	#ifdef  IS_WINDOWS_OS
		mSerial.OpenPort(com);
		mSerial.AddCommParser(&mATParser);
		mSerial.AddCommParser(&mM8218Parser);
		//bind Network communication failure processing function
		//绑定网络通讯失败处理函数 
		SRICommNetworkFailureCallbackFunction networkFailureCallback = std::bind(&CSRICommManager::OnNetworkFailure, this, std::placeholders::_1);
		mSerial.SetNetworkFailureCallbackFunction(networkFailureCallback);
	#else
		mSerial_linux.OpenSerialPort(ttyUSB0);
		mSerial_linux.AddCommParser(&mATParser);
		mSerial_linux.AddCommParser(&mM8218Parser);
		//bind Network communication failure processing function
		//绑定网络通讯失败处理函数 
		SRICommNetworkFailureCallbackFunction networkFailureCallback = std::bind(&CSRICommManager::OnNetworkFailure, this, std::placeholders::_1);
		mSerial_linux.SetNetworkFailureCallbackFunction(networkFailureCallback);
	#endif




	//bind ACK command Processing function
	//绑定ACK指令处理函数   	
	SRICommATCallbackFunction atCallback = std::bind(&CSRICommManager::OnCommACK, this, std::placeholders::_1);
	mATParser.SetATCallbackFunction(atCallback);

	//bind M8128 Data processing function
	//绑定M8128数据处理函数
	SRICommM8218CallbackFunction m8218Callback = std::bind(&CSRICommManager::OnCommM8218, this, 
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, 
		std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);
	mM8218Parser.SetM8218CallbackFunction(m8218Callback);

	return true;
}
//run
//启动
bool CSRICommManager::Run(int BaudRate, int sampling_freq)
{
	#ifdef  IS_WINDOWS_OS
		mSerial.run(BaudRate);
	#else
	mSerial_linux.SerialRun(BaudRate);
	#endif


	//向m8128发送指令，设置采样率
	std::string str = std::to_string(sampling_freq);
	const char* cstr = str.c_str();
	if (SendCommand("SMPF", cstr) == false)
	{
		if (SendCommand("GSD", "STOP") == false)
		{
		}
		if (SendCommand("SMPF", cstr) == false)
		{
			return false;
		}
	}


	//向m8128发送指令，设置数据校验模式
	if (SendCommand("DCKMD", "SUM") == false)
	{
		return false;
	}

	//向m8128发送指令，开始连续上传传感器数据
	
	if (SendCommand("GSD", "") == false)
	{
		//GSD has no ACK
		//return false;
	}

	return true;
}

bool CSRICommManager::ResetSensorZero()
{	//向m8128发送指令，将传感器数据调零
	if (SendCommand("ADJZF", "1;1;1;1;1;1") == false)
	{
		int aa = 0;
		//GSD cant reset
		//return false;
	}
	int bb = 0;
	return true;
}

bool CSRICommManager::Stop()
{
	#ifdef  IS_WINDOWS_OS
		mSerial.Close_Com();
	#else
		mSerial_linux.ClosePort();
	#endif	
	return true;
}

//通讯异常函数
bool CSRICommManager::OnNetworkFailure(std::string infor)
{
	printf("OnNetworkFailure = %s\n", infor.data());
	return true;
}


//send command 
//发送命令
bool CSRICommManager::SendCommand(std::string command, std::string parames)
{
	int CommandLenght;
	mIsGetACK = false;
	mCommandACK = "";
	mParamesACK = "";

	//Combination command
	//组合AT指令
	std::string atCommand = "AT+" + command + "=" + parames + "\r\n";
	CommandLenght = atCommand.length();

#ifdef  IS_WINDOWS_OS
	mSerial.ComWrite((BYTE*)atCommand.data(), CommandLenght);
	std::clock_t start = clock();
	while (true)
	{
		if (mIsGetACK == true)
		{
			break;
		}
		std::clock_t end = clock();
		long span = end - start;
		if (span >= 3000)//10s
		{
			return false;
		}
	}
#else
	mSerial_linux.Write((char*)atCommand.data(), CommandLenght);
	timeval start, end;
	gettimeofday(&start, NULL);
	while (true)
	{
		if (mIsGetACK == true)
		{
			break;
		}
		gettimeofday(&end, NULL);
		long span = 1000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000;
		if (span >= 10000)//10s
		{
			return false;
		}
	}
#endif
	//
	if (mCommandACK != command)
	{
		//ACK command error
		return false;
	}
	//
	printf("ACK+%s=%s", mCommandACK.data(), mParamesACK.data());
	//
	return true;
}

//ACK command processing
//ACK应答处理
bool CSRICommManager::OnCommACK(std::string command)
{
	int index = (int)command.find('=');
	if (index == -1)
	{
		mCommandACK = command;
		mParamesACK = "";
	}
	else
	{
		mCommandACK = command.substr(0, index);
		mParamesACK = command.substr(index+1);
	}
	mCommandACK = mCommandACK.substr(4);	
	
	mIsGetACK = true;//应答正确

	return true;
}
//M8128 data
//M8128数据
bool CSRICommManager::OnCommM8218(float fx, float fy, float fz, float mx, float my, float mz)
{
	printf("M8218 = %f, %f, %f,   %f, %f, %f\n", fx, fy, fz, mx, my, mz);
    std::lock_guard<std::mutex> lock(mtx);
	fx_main = fx;
	fy_main = fy;
	fz_main = fz;
	mx_main = mx;
	my_main = my;
	mz_main = mz;
	return true;
}