#ifndef SRI_COMM_MANAGER_H
#define SRI_COMM_MANAGER_H

#include "sriCommDefine.h"
#include "sriCommATParser.h"
#include "sriCommM8218Parser.h"

#ifdef IS_WINDOWS_OS
#include "sriCommSerial.h"
#else
#include "sriSerialManager_linux.h"
#endif
#include <mutex>

class CSRICommManager
{
public:
    CSRICommManager();
    ~CSRICommManager();

    bool Init(std::string com);
    bool Run(int BaudRate, int sampling_freq);
    bool Stop();

    bool SendCommand(std::string command, std::string parames);

    bool OnNetworkFailure(std::string infor);                                     // 通讯失败
    bool OnCommACK(std::string command);                                          // ACK应答数据处理
    bool OnCommM8218(float fx, float fy, float fz, float mx, float my, float mz); // GSD数据处理

    bool ResetSensorZero(); // 传感器置零

    void GetSensorData(float &fx, float &fy, float &fz, float &mx, float &my, float &mz)
    {
        std::lock_guard<std::mutex> lock(mtx);
        fx = fx_main;
        fy = fy_main;
        fz = fz_main;
        mx = mx_main;
        my = my_main;
        mz = mz_main;
    }
protected:
    std::mutex mtx;
    float fx_main;
    float fy_main;
    float fz_main;
    float mx_main;
    float my_main;
    float mz_main;

private:
#ifdef IS_WINDOWS_OS
    CSRICommSerial mSerial;
#else
    SerialManeger_linux mSerial_linux;
#endif

    CSRICommATParser mATParser;       // AT指令解析器
    CSRICommM8218Parser mM8218Parser; // GSD数据解析器

    bool mIsGetACK;
    std::string mCommandACK;
    std::string mParamesACK;
};

#endif
