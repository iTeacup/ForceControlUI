#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "UIURTeleop.h"
#include "UIBLEMonitor.h"
#include "UICfgParser.h"
#include "URManager.h"
#include "imgui/imgui.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include <cstdio>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <vector>

namespace ur_teleop {
inline double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
inline std::vector<double> Numbers(const nlohmann::json& j) {
    auto v = j.is_number() ? std::vector<double>{j.get<double>()} : j.get<std::vector<double>>();
    for (double x : v) Require(std::isfinite(x), "Nonfinite model parameter");
    return v;
}
class Model {
    struct Layer { size_t in, out; std::vector<double> w, b; bool nonlinear; };
    std::vector<Layer> layers;
    std::vector<double> offset, gain;
    double inputMin=0, outputOffset=0, outputGain=1, outputMin=0;
public:
    double Predict(const std::array<double,4>& delta) const {
        Require(!layers.empty(), "Load a model first");
        std::vector<double> a(4);
        for (int i=0;i<4;++i) {
            Require(std::isfinite(delta[i]), "Nonfinite voltage");
            a[i]=(delta[i]-offset[i])*gain[i]+inputMin;
            Require(std::isfinite(a[i]), "Nonfinite normalized voltage");
        }
        for (const auto& l:layers) {
            std::vector<double> b=l.b;
            for (size_t r=0;r<l.out;++r) {
                for (size_t c=0;c<l.in;++c) b[r]+=l.w[r*l.in+c]*a[c];
                if(l.nonlinear) b[r]=std::tanh(b[r]);
            }
            a=std::move(b);
        }
        double angle=(a[0]-outputMin)/outputGain+outputOffset;
        Require(std::isfinite(angle), "Nonfinite predicted angle");
        return angle;
    }
    void Load(const std::filesystem::path& path) {
        std::ifstream f(path); Require(bool(f), "Cannot open model JSON");
        nlohmann::json j; f>>j;
        Require(j.at("schema")=="elbow_bp_v1", "Unsupported model schema");
        Require(j.at("input_order")==nlohmann::json::array({"delta_ch1_mV","delta_ch2_mV","delta_ch3_mV","delta_ch4_mV"}), "Wrong channel order");
        Model m;
        m.offset=Numbers(j.at("input_offset"));m.gain=Numbers(j.at("input_gain"));
        Require(m.offset.size()==4 && m.gain.size()==4, "Expected four inputs");
        for(double g:m.gain) Require(g>0,"Invalid input gain");
        m.inputMin=j.at("input_ymin").get<double>();
        m.outputOffset=j.at("output_offset").get<double>();
        m.outputGain=j.at("output_gain").get<double>();
        m.outputMin=j.at("output_ymin").get<double>();
        Require(std::isfinite(m.inputMin)&&std::isfinite(m.outputOffset)&&std::isfinite(m.outputMin)&&std::isfinite(m.outputGain)&&m.outputGain>0,"Invalid scaling");
        size_t input=4;
        for(const auto& l:j.at("layers")) {
            Layer x; x.in=l.at("in").get<size_t>(); x.out=l.at("out").get<size_t>();
            Require(x.in==input && x.out>=1 && x.out<=128 && m.layers.size()<8,"Invalid network dimensions");
            x.w=Numbers(l.at("weights"));x.b=Numbers(l.at("bias"));
            Require(x.w.size()==x.in*x.out && x.b.size()==x.out,"Invalid weight dimensions");
            auto activation=l.at("activation").get<std::string>();
            Require(activation=="tansig"||activation=="purelin","Unsupported activation");
            x.nonlinear=activation=="tansig";input=x.out;m.layers.push_back(std::move(x));
        }
        Require(!m.layers.empty() && input==1 && !m.layers.back().nonlinear,"Expected linear scalar output");
        for(size_t i=0;i+1<m.layers.size();++i) Require(m.layers[i].nonlinear,"Expected tansig hidden layers");
        Require(j.at("checks").is_array() && !j.at("checks").empty(),"Missing model verification samples");
        for(const auto& check:j.at("checks")) {
            auto a=Numbers(check.at("delta_mV"));Require(a.size()==4,"Invalid verification sample");
            double expected=check.at("angle_deg").get<double>();
            Require(std::isfinite(expected)&&std::abs(m.Predict({a[0],a[1],a[2],a[3]})-expected)<1e-6,"Model verification failed");
        }
        *this=std::move(m);
    }
};
// Reject invalid numbers before smoothing; apply the work range to its final output.
constexpr double PredictionTimeout = 0.2;
class AngleSignal
{
public:
    enum class Rejection { None, Raw, Range, Settings };
private:
    double filtered = 0, lastSample = 0, rejectedSince = 0;
    bool initialized = false, rejecting = false;
    Rejection rejection = Rejection::None;
    bool Reject(double host, Rejection why)
    {
        if (!rejecting) rejectedSince = host;
        rejecting = true;
        rejection = why;
        return false;
    }
public:
    void Reset()
    {
        filtered = lastSample = rejectedSince = 0;
        initialized = rejecting = false;
        rejection = Rejection::None;
    }
    bool Push(double raw, double host, double lower, double upper, double smoothingMs)
    {
        Require(std::isfinite(host) && host > 0 && (lastSample == 0 || host > lastSample), "Invalid signal timestamp");
        const double dt = lastSample > 0 ? std::min(host - lastSample, 0.05) : 0;
        lastSample = host; // A discarded raw frame never creates filter catch-up time.
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper ||
            !std::isfinite(smoothingMs) || smoothingMs < 0 || smoothingMs > 200)
            return Reject(host, Rejection::Settings);
        if (!std::isfinite(raw))
            return Reject(host, Rejection::Raw);
        double next = raw;
        if (initialized && smoothingMs > 0)
        {
            const double alpha = -std::expm1(-dt * 1000 / smoothingMs);
            next = (1 - alpha) * filtered + alpha * raw;
        }
        if (!std::isfinite(next)) return Reject(host, Rejection::Raw);
        // Keep filtering through a work-range violation so recovery can converge normally.
        // An out-of-range output is displayed but never published to the robot.
        filtered = next;
        initialized = true;
        if (filtered < lower || filtered > upper) return Reject(host, Rejection::Range);
        rejecting = false;
        rejectedSince = 0;
        rejection = Rejection::None;
        return true;
    }
    double Value() const { return filtered; }
    bool Initialized() const { return initialized; }
    Rejection Reason() const { return rejection; }
    double RejectedSince() const { return rejectedSince; }
    bool Expired(double now) const { return rejecting && now - rejectedSince >= PredictionTimeout; }
};
// Positions are radians internally; all configuration angles and rates are degrees.
// Only the selected target can change. Absolute soft limits apply before rate limiting.
class JointTarget {
    std::vector<double> start;
    int joint, sign;
    double robotZero, lower, upper, speed, current;
public:
    JointTarget(const std::vector<double>& q,int j,int direction,double zero,
                double lowerDeg,double upperDeg,double maxSpeed)
        :start(q),joint(j),sign(direction),robotZero(zero),lower(lowerDeg),upper(upperDeg),speed(maxSpeed),current(0) {
        Require(q.size()==6&&j>=0&&j<6,"Invalid selected joint");
        for(double v:q) Require(std::isfinite(v),"Invalid initial joints");
        Require((direction==1||direction==-1)&&std::isfinite(zero)&&std::isfinite(lowerDeg)&&std::isfinite(upperDeg)&&
            lowerDeg<upperDeg&&std::isfinite(maxSpeed)&&maxSpeed>0&&maxSpeed<=30,
            "Invalid direction, zero, soft limits or speed");
        constexpr double deg=180/3.14159265358979323846;
        current=q[j]*deg;
        Require(current>=lower&&current<=upper,"Current joint angle is outside soft limits");
    }
    double Desired(double angle) const {
        Require(std::isfinite(angle),"Invalid elbow angle");
        return std::clamp(robotZero+sign*angle,lower,upper);
    }
    bool Inside(double radians) const {
        constexpr double deg=180/3.14159265358979323846;
        // Small feedback tolerance avoids stopping on numerical boundary noise.
        return std::isfinite(radians)&&radians*deg>=lower-0.1&&radians*deg<=upper+0.1;
    }
    std::vector<double> Next(double angle,double dt) {
        Require(std::isfinite(angle)&&dt>0&&dt<=0.05,"Invalid target input");
        current+=std::clamp(Desired(angle)-current,-speed*dt,speed*dt);
        current=std::clamp(current,lower,upper);
        return Hold();
    }
    std::vector<double> Hold() const {
        auto q=start;q[joint]=current*3.14159265358979323846/180;return q;
    }
};
}

UIURTeleop::UIURTeleop(UIMainWindowBase* main,const char* title,UIBLEMonitor* b,URManager* r)
    :UIBaseWindow(main,title),ble(b),ur(r),model(std::make_unique<ur_teleop::Model>()),signal(std::make_unique<ur_teleop::AngleSignal>()) {
    const auto& lastPath = g_cfg->m_ur_model_path;
    if (!lastPath.empty() && lastPath.size() < sizeof(modelPath))
        std::snprintf(modelPath, sizeof(modelPath), "%s", lastPath.c_str());
    else
        std::snprintf(modelPath,sizeof(modelPath),"%s",
            u8R"(C:\Users\qitia\Desktop\具身智能\肘关节神经网络学习\BP神经网络脚本\训练结果\elbow_bp_model.json)");
}
UIURTeleop::~UIURTeleop() { Shutdown(); }
void UIURTeleop::SetStatus(const std::string& s) { std::lock_guard<std::mutex> lock(mutex);status=s; }
void UIURTeleop::Stop() {
    running=false;
    if(worker.joinable()) worker.join();
}
void UIURTeleop::Shutdown() {
    if(closed) return;
    Stop();closed=true;
}
void UIURTeleop::ResetPrediction()
{
    signal->Reset();
    predictionValid = false;
    rawAngle = std::numeric_limits<double>::quiet_NaN();
    std::lock_guard<std::mutex> lock(mutex);
    sharedHost = sharedRejectedAt = 0;
    sharedHold = true;
}
void UIURTeleop::Tick()
{
    UIBLEMonitor::Sample s;
    fresh = ble && ble->LatestSample(device, s) && ur_teleop::Now() - s.host <= 0.25;
    if (!fresh)
    {
        if (running) { Stop(); SetStatus(u8"蓝牙数据中断，已停止跟随，请重新标定"); }
        calibrated = false; calibrating = false;
        ResetPrediction();
        return;
    }
    if (s.generation != generation)
    {
        Stop(); calibrated = false; calibrating = false; lastHost = 0;
        ResetPrediction();
        generation = s.generation;
        SetStatus(u8"蓝牙连接已更新，请标定伸直零位");
    }
    if (s.host == lastHost) return;
    if (lastHost > 0 && static_cast<int32_t>(s.data.timestamp - lastStamp) <= 0)
    {
        Stop(); calibrated = false; calibrating = false; fresh = false;
        ResetPrediction();
        SetStatus(u8"蓝牙时间戳异常，请重新连接并标定");
        return;
    }
    lastHost = s.host; lastStamp = s.data.timestamp;
    bool validVoltage = true;
    for (int i = 0; i < 4; ++i)
    {
        voltage[i] = s.data.channel_mv[i];
        validVoltage = validVoltage && std::isfinite(voltage[i]);
    }
    if (calibrating)
    {
        if (!validVoltage)
        {
            calibrating = false; calibrated = false;
            ResetPrediction();
            SetStatus(u8"标定期间电压无效，请重试");
            return;
        }
        if (calibrationCount == 0) calibrationStart = s.host;
        ++calibrationCount;
        for (int i = 0; i < 4; ++i) { sum[i] += voltage[i]; square[i] += voltage[i] * voltage[i]; }
        if (s.host - calibrationStart >= 2.0)
        {
            calibrated = calibrationCount >= 30;
            for (int i = 0; i < 4; ++i)
            {
                baseline[i] = sum[i] / calibrationCount;
                double variance = std::max(0.0, square[i] / calibrationCount - baseline[i] * baseline[i]);
                if (std::sqrt(variance) > 5.0) calibrated = false;
            }
            calibrating = false;
            if (calibrated)
            {
                try { predictionZero = model->Predict({0, 0, 0, 0}); }
                catch (const std::exception& e)
                {
                    calibrated = false; ResetPrediction(); SetStatus(e.what()); return;
                }
            }
            SetStatus(calibrated ? u8"零位标定完成，可预览或开始跟随" : u8"标定不稳定或采样不足，请伸直保持不动后重试");
        }
    }
    if (!loaded || !calibrated) return;

    // Once the grace period elapsed, recovery must not restart a running session.
    const bool expired = signal->Expired(ur_teleop::Now());
    if (expired && running)
    {
        Stop();
        SetStatus(u8"预测持续异常，已停止；零位保留，请手动重新开始");
    }
    rawAngle = std::numeric_limits<double>::quiet_NaN();
    std::string reason;
    try
    {
        ur_teleop::Require(validVoltage, u8"电压无效");
        std::array<double, 4> delta{};
        for (int i = 0; i < 4; ++i) delta[i] = voltage[i] - baseline[i];
        rawAngle = model->Predict(delta) - predictionZero;
    }
    catch (const std::exception& e) { reason = e.what(); }

    const bool wasValid = predictionValid;
    const auto previousRejection = signal->Reason();
    const bool accepted = signal->Push(rawAngle, s.host, predictionLower, predictionUpper, smoothingMs);
    if (signal->Initialized()) angle = signal->Value();
    if (accepted)
    {
        predictionValid = true;
        if (!wasValid)
            SetStatus(running ? u8"信号恢复，继续跟随" : u8"预测有效，零位保留；可手动开始跟随");
        std::lock_guard<std::mutex> lock(mutex);
        sharedAngle = angle; sharedHost = s.host;
        sharedHold = false; sharedRejectedAt = 0;
    }
    else
    {
        predictionValid = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            sharedHold = true;
            sharedRejectedAt = signal->RejectedSince();
            // Do not refresh sharedHost: the worker still watches the last valid prediction.
        }
        if (signal->Expired(ur_teleop::Now()))
        {
            Stop();
            SetStatus(u8"预测持续异常，已停止；零位保留，请手动重新开始");
        }
        else if (wasValid || previousRejection != signal->Reason())
        {
            if (reason.empty())
            {
                using Rejection = ur_teleop::AngleSignal::Rejection;
                reason = signal->Reason() == Rejection::Raw ? u8"预测值不是有效数值" :
                         signal->Reason() == Rejection::Settings ? u8"预测设置无效" : u8"最终控制角度超出工作范围";
            }
            SetStatus(std::string(u8"短暂预测异常，保持已下发目标：") + reason);
        }
    }
}
void UIURTeleop::Start() {
    Stop();
    if(!loaded||!calibrated||!predictionValid||!fresh||!ur) return;
    if(!std::isfinite(predictionLower)||!std::isfinite(predictionUpper)||predictionLower>=predictionUpper||
        !std::isfinite(angle)||angle<predictionLower||angle>predictionUpper) {
        SetStatus(u8"请设置有效的肘关节工作范围");return;
    }
    if(!std::isfinite(lowerLimit)||!std::isfinite(upperLimit)||!std::isfinite(robotZero)||
        lowerLimit>=upperLimit) {
        SetStatus(u8"设置无效：须下限 < 上限，且零位和限位均为有效数值");return;
    }
    std::vector<double> initial;
    if(!ur->BeginJointTeleop(joint,initial)) {
        SetStatus(u8"无法开始：请确认机器人已连接、静止且处于可运行状态");return;
    }
    try {
        ur_teleop::JointTarget trajectory(initial,joint,direction==0?1:-1,robotZero,lowerLimit,upperLimit,maxSpeed);
        {std::lock_guard<std::mutex> lock(mutex);targetRad=initial[joint];}
        running=true;SetStatus(u8"跟随中，其余五个关节目标保持启动位置");
        worker=std::thread([this,trajectory,initial,selected=joint]() mutable {
            auto previous=std::chrono::steady_clock::now()-std::chrono::milliseconds(8);
            try {
                while(running) {
                    auto cycle=std::chrono::steady_clock::now();
                    const double elapsed=std::chrono::duration<double>(cycle-previous).count();
                    ur_teleop::Require(elapsed>0&&elapsed<0.1,"Control loop timeout");
                    // Real elapsed time; cap catch-up after a delayed cycle.
                    const double dt=std::min(elapsed,0.05);
                    previous=cycle;
                    double a,h,rejectedAt;
                    bool hold;
                    {std::lock_guard<std::mutex> lock(mutex);
                        a=sharedAngle;h=sharedHost;hold=sharedHold;rejectedAt=sharedRejectedAt;
                    }
                    ur_teleop::Require(!hold || (rejectedAt>0 && ur_teleop::Now()-rejectedAt<ur_teleop::PredictionTimeout),
                        u8"预测持续异常，零位保留，请手动重新开始");
                    ur_teleop::Require(h>0&&ur_teleop::Now()-h<=0.25,"BLE/prediction timeout");
                    ST_URDataFrame frame{};
                    ur_teleop::Require(ur->TeleopFeedback(frame),"Robot feedback unavailable");
                    for(int i=0;i<6;++i)
                        if(i!=selected) ur_teleop::Require(std::abs(frame.joint_info[i]-initial[i])<0.05,"Unselected joint moved");
                    ur_teleop::Require(trajectory.Inside(frame.joint_info[selected]),"Joint outside soft limits");
                    auto q=hold?trajectory.Hold():trajectory.Next(a,dt);
                    ur_teleop::Require(std::abs(frame.joint_info[selected]-q[selected])<0.25,"Joint tracking error");
                    ur_teleop::Require(ur->ServoJointTeleop(q[selected],0.008),"Robot servo rejected/disconnected");
                    {std::lock_guard<std::mutex> lock(mutex);targetRad=q[selected];}
                    std::this_thread::sleep_until(cycle+std::chrono::milliseconds(8));
                }
                SetStatus(u8"跟随已停止");
            } catch(const std::exception& e) {SetStatus(std::string(u8"跟随停止：")+e.what());}
            ur->EndJointTeleop();running=false;
        });
    } catch(const std::exception& e) {
        running=false;ur->EndJointTeleop();SetStatus(e.what());
    }
}
namespace
{
constexpr double Degrees = 180.0 / 3.14159265358979323846;
const char* JointNames[] = {"J1 Base", "J2 Shoulder", "J3 Elbow", "J4 Wrist 1", "J5 Wrist 2", "J6 Wrist 3"};
const ImVec4 ReadyColor(0.35f, 0.85f, 0.65f, 1.0f);

void Field(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
}

bool DegreeRange(const char* id, double& lower, double& upper, const char* format)
{
    ImGui::PushID(id);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float separator = ImGui::CalcTextSize(u8"～").x;
    const float width = std::max(1.0f, (ImGui::GetContentRegionAvail().x - separator - gap * 2) / 2);
    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::InputDouble("##Lower", &lower, 0, 0, format);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"下限");
    ImGui::SameLine(); ImGui::TextUnformatted(u8"～"); ImGui::SameLine();
    ImGui::SetNextItemWidth(width);
    changed |= ImGui::InputDouble("##Upper", &upper, 0, 0, format);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"上限");
    ImGui::PopID();
    return changed;
}
}

void UIURTeleop::DrawBLE(bool active)
{
    ImGui::SeparatorText(u8"BLE · 肘关节预览");
    ImGui::BeginDisabled(active);
    const auto devices = ble ? ble->Devices() : std::vector<UIBLEMonitor::DeviceInfo>{};
    const UIBLEMonitor::DeviceInfo* chosen = nullptr;
    for (const auto& d : devices)
        if (d.id == device) chosen = &d;
    const float connectWidth = ImGui::CalcTextSize(u8"连接 BLE").x + ImGui::GetStyle().FramePadding.x * 2;
    const float settingsWidth = ImGui::CalcTextSize(u8"设置").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - connectWidth - settingsWidth - ImGui::GetStyle().ItemSpacing.x * 2));
    if (ImGui::BeginCombo("##BLEDevice", chosen ? chosen->name.c_str() : u8"请选择 BLE 设备"))
    {
        for (const auto& d : devices)
        {
            const auto label = d.name + "##" + std::to_string(d.id);
            if (ImGui::Selectable(label.c_str(), d.id == device))
            {
                device = d.id; chosen = &d;
                calibrated = false; calibrating = false;
                generation = 0; lastHost = 0;
                ResetPrediction();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!chosen || chosen->busy);
    if (ImGui::Button(chosen && chosen->connected ? u8"断开 BLE" : u8"连接 BLE", ImVec2(connectWidth, 0)))
    {
        ble->ConnectDevice(device, !chosen->connected);
        calibrated = false; calibrating = false;
        ResetPrediction();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(u8"设置", ImVec2(settingsWidth, 0)) && ble)
    {
        ble->Show();
        ImGui::SetWindowFocus(ble->GetWinTitle());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"在蓝牙监控中设置广播名和 UUID，连接与同步采集共用。");

    const float modelButton = ImGui::CalcTextSize(u8"加载").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - modelButton * 2 - ImGui::GetStyle().ItemSpacing.x * 2));
    ImGui::InputText("##ModelPath", modelPath, sizeof(modelPath));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"角度预测模型：\n%s", modelPath);
    ImGui::SameLine();
    if (ImGui::Button(u8"选择", ImVec2(modelButton, 0)))
    {
        IGFD::FileDialogConfig config;
        config.path = std::filesystem::u8path(modelPath).parent_path().u8string();
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("URTeleopModel", u8"选择肘关节模型", ".json", config);
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"加载", ImVec2(modelButton, 0)))
    {
        calibrated = false; calibrating = false; loaded = false;
        ResetPrediction();
        try
        {
            const auto path = std::filesystem::absolute(std::filesystem::u8path(modelPath)).lexically_normal().u8string();
            ur_teleop::Require(path.size() < sizeof(modelPath), "Model path is too long");
            model->Load(std::filesystem::u8path(path));
            std::snprintf(modelPath, sizeof(modelPath), "%s", path.c_str());
            loaded = true;
            const auto previousPath = g_cfg->m_ur_model_path;
            g_cfg->m_ur_model_path = path;
            if (g_cfg->SaveCfg()) SetStatus(u8"模型已加载，请伸直标定零位");
            else
            {
                g_cfg->m_ur_model_path = previousPath;
                SetStatus(u8"模型已加载，但路径保存失败，请检查配置文件写入权限");
            }
        }
        catch (const std::exception& e) { SetStatus(std::string(u8"模型加载失败：") + e.what()); }
    }
    if (calibrating)
        ImGui::TextDisabled(u8"保持伸直 %.1f / 2 s", calibrationCount ? std::clamp(lastHost - calibrationStart, 0.0, 2.0) : 0);
    else
        ImGui::TextDisabled(u8"%s · %s", loaded ? u8"模型已加载" : u8"待加载模型",
                            !fresh ? u8"等待 BLE" : calibrated ? u8"零位已标定" : u8"待标定");
    ImGui::BeginDisabled(!fresh || !loaded || calibrating);
    if (ImGui::Button(u8"伸直保持 2 秒 · 标定零位", ImVec2(-1, 0)))
    {
        calibrated = false; calibrating = true;
        ResetPrediction();
        calibrationCount = 0; sum.fill(0); square.fill(0);
        SetStatus(u8"正在标定，请保持肘关节伸直不动");
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    ImGui::SeparatorText(u8"四通道电压");
    if (ImGui::BeginTable("BLEVoltages", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchSame))
    {
        for (const char* label : {u8"通道", u8"电压 mV", u8"零位 mV", u8"变化 mV"}) ImGui::TableSetupColumn(label);
        ImGui::TableHeadersRow();
        for (int i = 0; i < 4; ++i)
        {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("CH%d", i + 1);
            ImGui::TableNextColumn(); if (fresh) ImGui::Text("%.2f", voltage[i]); else ImGui::TextDisabled("--");
            ImGui::TableNextColumn(); if (calibrated) ImGui::Text("%.2f", baseline[i]); else ImGui::TextDisabled("--");
            ImGui::TableNextColumn(); if (fresh && calibrated) ImGui::Text("%+.2f", voltage[i] - baseline[i]); else ImGui::TextDisabled("--");
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText(u8"平滑与范围");
    if (ImGui::BeginTable("BLEOptions", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        Field(u8"平滑时间");
        smoothingMs = std::isfinite(smoothingMs) ? std::clamp(smoothingMs, 0.0f, 200.0f) : 50.0f;
        ImGui::AlignTextToFramePadding(); ImGui::TextDisabled(u8"灵敏"); ImGui::SameLine();
        const float steadyWidth = ImGui::CalcTextSize(u8"平稳").x + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - steadyWidth));
        ImGui::SliderFloat("##Smoothing", &smoothingMs, 0, 200, "%.0f ms", ImGuiSliderFlags_AlwaysClamp);
        smoothingMs = std::isfinite(smoothingMs) ? std::clamp(smoothingMs, 0.0f, 200.0f) : 50.0f;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(u8"默认 50 ms；0 ms 关闭。向右更平稳，也会更迟缓。\n滤波时间常数，表示响应快慢，每帧都会更新。");
        ImGui::SameLine(); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled(u8"平稳");
        Field(u8"肘角范围");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(u8"工作范围检查最终控制角度。短暂异常冻结目标，持续 200 ms 异常停止，零位保留。\n平滑为 0 时检查归零原始值；开启平滑时检查滤波值。");

        ImGui::BeginDisabled(active);
        if (DegreeRange("PredictionRange", predictionLower, predictionUpper, u8"%.1f °")) ResetPrediction();
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    if (!std::isfinite(predictionLower) || !std::isfinite(predictionUpper) || predictionLower >= predictionUpper)
        ImGui::TextUnformatted(u8"肘角范围无效：下限须小于上限。");

    ImGui::SeparatorText(u8"角度预览");
    if (ImGui::BeginTable("AnglePreview", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(u8"归零后角度（°）");
        ImGui::TableSetupColumn(u8"最终控制角度（°）");
        ImGui::TableHeadersRow(); ImGui::TableNextRow(); ImGui::TableNextColumn();
        if (loaded && calibrated && fresh && std::isfinite(rawAngle)) ImGui::Text("%.2f", rawAngle);
        else ImGui::TextDisabled("--");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"模型输出减去伸直标定时的输出，尚未平滑。");
        ImGui::TableNextColumn();
        if (loaded && calibrated && fresh && signal->Initialized())
            ImGui::TextColored(predictionValid ? ReadyColor : ImVec4(1, .75f, .35f, 1),
                               u8"%.2f%s", angle, predictionValid ? "" : u8"（目标保持）");
        else ImGui::TextDisabled("--");
        ImGui::EndTable();
    }
}

void UIURTeleop::DrawUR(bool active, const ST_URDataFrame* frame)
{
    ImGui::SeparatorText(u8"UR · 关节与映射");
    ImGui::BeginDisabled(active);
    const float buttonWidth = ImGui::CalcTextSize(u8"连接 UR").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x));
    ImGui::InputText("##RobotIP", hostname, sizeof(hostname)); ImGui::SameLine();
    if (ImGui::Button(ur->IsConnected() ? u8"断开 UR" : u8"连接 UR", ImVec2(buttonWidth, 0)))
    {
        if (ur->IsConnected()) ur->Disconnect();
        else SetStatus(ur->Connect(hostname) ? u8"机器人已连接" : u8"机器人连接失败");
    }
    ImGui::EndDisabled();
    if (ImGui::BeginTable("RobotJoints", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
    {
        ImGui::TableSetupColumn(u8"跟随关节", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(u8"当前角度（°）", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (int i = 0; i < 6; ++i)
        {
            ImGui::PushID(i); ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::BeginDisabled(active);
            if (ImGui::RadioButton(JointNames[i], joint == i)) joint = i;
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (frame) ImGui::Text(u8"%.2f °", frame->joint_info[i] * Degrees); else ImGui::TextDisabled(u8"-- °");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::BeginDisabled(active);
    if (ImGui::BeginTable("MappingSettings", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.5f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        Field(u8"跟随方向");
        ImGui::Combo("##Direction", &direction, u8"正向（+）\0反向（-）\0\0");
        Field(u8"机器人零位");
        const float zeroButton = ImGui::CalcTextSize(u8"取当前值").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - zeroButton - ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputDouble("##RobotZero", &robotZero, 0, 0, u8"%.2f °"); ImGui::SameLine();
        ImGui::BeginDisabled(!frame);
        if (ImGui::Button(u8"取当前值")) robotZero = frame->joint_info[joint] * Degrees;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"肘关节 0° 对应的机器人角度。取当前所选关节角度为零位。");
        Field(u8"UR 软限位");
        DegreeRange("RobotLimits", lowerLimit, upperLimit, u8"%.2f °");
        Field(u8"最大速度");
        ImGui::SliderFloat("##MaxSpeed", &maxSpeed, 1, 30, u8"%.1f °/s");
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
}

void UIURTeleop::DrawControl(bool active, const ST_URDataFrame* frame)
{
    const bool validLimits = std::isfinite(lowerLimit) && std::isfinite(upperLimit) && std::isfinite(robotZero) && lowerLimit < upperLimit;
    const bool currentInside = frame && frame->joint_info[joint] * Degrees >= lowerLimit && frame->joint_info[joint] * Degrees <= upperLimit;
    const bool predicted = loaded && calibrated && predictionValid && fresh &&
        std::isfinite(predictionLower) && std::isfinite(predictionUpper) && predictionLower < predictionUpper;
    const bool ready = predicted && frame && validLimits && currentInside;
    ImGui::SeparatorText(u8"遥操控制 · 1:1");
    ImGui::BeginDisabled(active || !ready);
    if (ImGui::Button(u8"开始跟随", ImVec2(ImGui::GetFontSize() * 7, 0))) Start();
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.24f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.29f, 1));
    if (ImGui::Button(u8"停止跟随", ImVec2(ImGui::GetFontSize() * 7, 0))) Stop();
    ImGui::PopStyleColor(2); ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::Text(u8"%s · %s", running ? u8"跟随中" : u8"未跟随", JointNames[joint]);
    std::string currentStatus;
    double commandTarget;
    {
        std::lock_guard<std::mutex> lock(mutex);
        currentStatus = status; commandTarget = targetRad;
    }
    if (!running && !validLimits) currentStatus = u8"请设置有效的机器人零位和软限位，下限必须小于上限。";
    else if (!running && frame && !currentInside) currentStatus = u8"当前关节在软限位外，请调整限位或先手动移回限位内。";
    if (ImGui::BeginTable("TeleopOverview", 4, ImGuiTableFlags_SizingStretchSame))
    {
        for (const char* label : {u8"已下发目标", u8"限位后目标", u8"实际速度", u8"UR 速度比例"}) ImGui::TableSetupColumn(label);
        ImGui::TableHeadersRow(); ImGui::TableNextRow(); ImGui::TableNextColumn();
        if (running) ImGui::Text(u8"%.2f °", commandTarget * Degrees); else ImGui::TextDisabled(u8"-- °");
        ImGui::TableNextColumn();
        if (predicted && validLimits)
            ImGui::Text(u8"%.2f °", std::clamp(robotZero + (direction == 0 ? angle : -angle), lowerLimit, upperLimit));
        else ImGui::TextDisabled(u8"-- °");
        ImGui::TableNextColumn();
        if (frame) ImGui::Text(u8"%.1f °/s", frame->joint_speed[joint] * Degrees); else ImGui::TextDisabled(u8"-- °/s");
        ImGui::TableNextColumn();
        double scaling;
        if (frame && ur->SpeedScaling(scaling)) ImGui::Text("%.0f%%", scaling * 100); else ImGui::TextDisabled("--");
        ImGui::EndTable();
    }
    ImGui::TextUnformatted(currentStatus.c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", currentStatus.c_str());
}

void UIURTeleop::Draw()
{
    if (closed) return;
    if (!m_show) { if (running) Stop(); return; }
    Tick();
    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End(); if (running) Stop(); return;
    }
    ST_URDataFrame frame{};
    const bool feedback = ur->IsConnected() && ur->RobotFeedback(frame);
    const auto robotFrame = feedback ? &frame : nullptr;
    const bool active = running.load();
    const bool compact = ImGui::GetContentRegionAvail().y < ImGui::GetFontSize() * 28;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, compact ? 2.0f : 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, compact ? 1.0f : 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(5, compact ? 1.0f : 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 5 +
                         ImGui::GetStyle().WindowPadding.y * 2 + ImGui::GetStyle().CellPadding.y * 4;
    const float panelHeight = std::max(100.0f, ImGui::GetContentRegionAvail().y - footer - ImGui::GetStyle().ItemSpacing.y);
    const bool twoColumns = ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 40;
    if (twoColumns && ImGui::BeginTable("TeleopPanels", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::BeginChild("BLEPanel", ImVec2(0, panelHeight), true);
        DrawBLE(active); ImGui::EndChild(); ImGui::TableNextColumn();
        ImGui::BeginChild("URPanel", ImVec2(0, panelHeight), true);
        DrawUR(active, robotFrame); ImGui::EndChild(); ImGui::EndTable();
    }
    else if (!twoColumns)
    {
        ImGui::BeginChild("TeleopNarrow", ImVec2(0, panelHeight), true);
        if (ImGui::BeginTabBar("TeleopPages"))
        {
            if (ImGui::BeginTabItem(u8"BLE 与肘角")) { DrawBLE(active); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(u8"UR 与映射")) { DrawUR(active, robotFrame); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
    }
    ImGui::BeginChild("TeleopControl", ImVec2(0, 0), true);
    DrawControl(active, robotFrame); ImGui::EndChild();
    ImGui::PopStyleVar(4);
    if (ImGuiFileDialog::Instance()->Display("URTeleopModel", ImGuiWindowFlags_NoCollapse, ImVec2(800, 500)))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const auto path = ImGuiFileDialog::Instance()->GetFilePathName();
            std::snprintf(modelPath, sizeof(modelPath), "%s", path.c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::End();
    if (!m_show && running) Stop();
}
