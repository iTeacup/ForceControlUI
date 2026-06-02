#pragma once
#include <vector>
#include <string>
#include "UIWindowManagerBase.h"
#include "UILogView.h"
#include "UISysSettings.h"
#include "UIImageView.h"
#include "UIIconsFontView.h"

class UIMainWindow;
class UIApplication;

class UIConnectionSettings;
class UISerialCon;
class UIADCMonitor;
class UICOMMonitor;
class UIBLEMonitor;
class UIOptSampling;
class UIMotorControl;
class UIURobot;
class UIOptiTrack;
class UIHypersenMonitor;
class UIYHGripper;
class UIPipeRobot;
class UILQYExp;
class UIADCSerialCon;
class UIOtherSerialCon;
class UIBeamCalibration;

class UIWindowManager: public UIWindowManagerBase
{
public:
	UIWindowManager(UIMainWindow * ui_main);
	~UIWindowManager();

	void Init() override;

	void Draw() override;

	void OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len) override;

	void Destroy() override;
	
	UISysSettings* GetSysSettings() { return m_ui_sys; }
	UIConnectionSettings* GetConnectionSettings() { return m_ui_conn; }
    UISerialCon* GetSerialCon() { return m_ui_serial; }
    UIHypersenMonitor* GetHypersenMonitor() { return m_ui_hypersen; }
    UIURobot* GetURobot() { return m_ui_urobot; }
    UIOptiTrack* GetOptiTrack() { return m_ui_opt; }
public:
	void ShowOpenModelFileDialog();
	void ShowSaveModelFileDialog();

	void OpenFile(const std::string& filename);
	void SaveFile(const std::string& filename);

	void SetResetLayoutFlag() { m_reset_layout = true; }

	void ShowAllWindow();
	void HideAllWindow();
	void SetShowDemo(bool show) { m_show_demo = show; }
#ifdef UI_CFG_USE_IMPLOT
	void SetShowPlotDemo(bool show) { m_show_plot_demo = show; }
#endif
protected:
	void InitWindows();
	void InitMenus();
	void InitImDialog();
	void InitShortCut();
	void ResetLayout();

	void ShowDemo();
#ifdef UI_CFG_USE_IMPLOT
	void ShowPlotDemo();
#endif
protected:
	// ´°¿ÚÁÐ±í
	std::vector<UIBaseWindow*> m_win_list;

	UILogView* m_ui_log = nullptr;
	UISysSettings* m_ui_sys = nullptr;
	UIImageView* m_ui_image = nullptr;
	UIIconsFontView* m_ui_icons = nullptr;
	UIConnectionSettings* m_ui_conn = nullptr;
    UISerialCon* m_ui_serial = nullptr;
	UIMotorControl* m_ui_motor = nullptr;
	UIADCMonitor* m_ui_adc = nullptr;

	UICOMMonitor* m_ui_com = nullptr;
	UIBLEMonitor* m_ui_ble_monitor = nullptr;
	UIOptSampling* m_ui_opt_sampling = nullptr;

    UIHypersenMonitor* m_ui_hypersen = nullptr;
    UIYHGripper* m_ui_yhgripper = nullptr;
    UIURobot* m_ui_urobot = nullptr;
	UIOptiTrack* m_ui_opt = nullptr;
    UIPipeRobot* m_ui_piperobot = nullptr;
    UILQYExp* m_ui_lqyexp = nullptr;
    UIADCSerialCon* m_ui_adc_serial = nullptr;
	UIOtherSerialCon* m_ui_other_serial = nullptr;
	UIBeamCalibration* m_ui_beamcalibration = nullptr;

	// window show
	bool m_show_demo = false;
#ifdef UI_CFG_USE_IMPLOT
	bool m_show_plot_demo = false;
#endif

	bool m_reset_layout = true;
};

using UIWindowManagerPtr = std::shared_ptr<UIWindowManager>;
