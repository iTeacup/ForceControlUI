#pragma once
#include "UIBaseWindow.h"
#include "URDef.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class UIBLEMonitor;
class URManager;
namespace ur_teleop { class Model; class AngleSignal; }

class UIURTeleop : public UIBaseWindow
{
public:
    UIURTeleop(UIMainWindowBase*, const char*, UIBLEMonitor*, URManager*);
    ~UIURTeleop() override;
    void Draw() override;
    void Shutdown();
    EUIMenuCategory GetWinMenuCategory() override { return EUIMenuCategory::E_UI_CAT_APP; }

private:
    void Tick();
    void Stop();
    void Start();
    void SetStatus(const std::string&);
    void ResetPrediction();
    void DrawBLE(bool active);
    void DrawUR(bool active, const ST_URDataFrame* frame);
    void DrawControl(bool active, const ST_URDataFrame* frame);

    UIBLEMonitor* ble;
    URManager* ur;
    std::unique_ptr<ur_teleop::Model> model;
    std::unique_ptr<ur_teleop::AngleSignal> signal;
    bool loaded = false, calibrated = false, predictionValid = false;
    bool calibrating = false, fresh = false, closed = false;
    uint32_t device = 1, lastStamp = 0;
    uint64_t generation = 0;
    double lastHost = 0, calibrationStart = 0, angle = 0, predictionZero = 0;
    double rawAngle = 0, predictionLower = -10, predictionUpper = 140;
    float smoothingMs = 50;
    size_t calibrationCount = 0;
    std::array<double, 4> voltage{}, baseline{}, sum{}, square{};
    char modelPath[1024] = {0};
    char hostname[64] = "192.168.12.10";
    int joint = 2, direction = 0;
    double robotZero = 0, lowerLimit = -180, upperLimit = 180;
    float maxSpeed = 10;
    std::atomic<bool> running{false};
    std::thread worker;
    std::mutex mutex;
    double sharedAngle = 0, sharedHost = 0, sharedRejectedAt = 0, targetRad = 0;
    bool sharedHold = true;
    std::string status = u8"请加载模型并连接蓝牙";
};
