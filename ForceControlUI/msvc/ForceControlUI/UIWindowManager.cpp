#include "UIWindowManager.h"
#include "UIMainWindow.h"
#include "UIApplication.h"
#include "IconsFontAwesome6.h"
#include "UIUtils.h"
#include "UIFontLoader.h"
#include "UITextureLoader.h"
#include "UIHelpAbout.h"

#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "ImGuiFileDialog/CustomFont.h"
#include "stb/stb_sprintf.h"

#include "imgui/imgui_internal.h"
#ifdef UI_CFG_USE_IMPLOT
#include "implot/implot.h"
#endif

#include "UIConnectionSettings.h"
#include "UISerialCon.h"
#include "UIMotorControl.h"
#include "UIADCMonitor.h"
#include "UIHypersenMonitor.h"
#include "UIYHGripper.h"
#include "UIURobot.h"
#include "UIOptiTrack.h"
#include "UIPipeRobot.h"
#include "UILQYExp.h"
#include "UIADCSerialCon.h"
#include "UIOtherSensors.h"
#include "UIBeamCalibration.h"
#include "UICOMMonitor.h"
#include "UIBLEMonitor.h"
#include "UIOptSampling.h"

UIWindowManager::UIWindowManager(UIMainWindow* ui_main):
	UIWindowManagerBase(ui_main)
{

}

UIWindowManager::~UIWindowManager()
{

}


void UIWindowManager::Init()
{
	// init imdialog
	InitImDialog();

	// create ui windows
	InitWindows();
	
	// init menus
	InitMenus();

	// init shortcut
	InitShortCut();
}

void UIWindowManager::Draw()
{
	ShowDemo();
#ifdef UI_CFG_USE_IMPLOT
	ShowPlotDemo();
#endif

	for (size_t i = 0; i < m_win_list.size(); i++)
	{
		m_win_list[i]->Draw();
	}

	// set initial layout
	if (m_reset_layout)
	{
		ResetLayout();
		m_reset_layout = false;
	}
	// display
	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(800, 640)))
	{
		// action if OK
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			// action
			OpenFile(filePathName);
		}
		// close
		ImGuiFileDialog::Instance()->Close();
	}
	// save 
	if (ImGuiFileDialog::Instance()->Display("SaveFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(800, 640)))
	{
		// action if OK
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			// action
			SaveFile(filePathName);
		}
		// close
		ImGuiFileDialog::Instance()->Close();
	}
}

void UIWindowManager::InitWindows()
{
	char buf[256] = { 0 };

	stbsp_sprintf(buf, u8"%s 系统设置", ICON_FA_GEARS);
	m_ui_sys = new UISysSettings(m_ui_main, buf);
	m_win_list.push_back(m_ui_sys);

	stbsp_sprintf(buf, u8"%s 图像", ICON_FA_IMAGE);
	m_ui_image = new UIImageView(m_ui_main, buf);
	m_win_list.push_back(m_ui_image);

	stbsp_sprintf(buf, u8"%s 日志", ICON_FA_PRINT);
	m_ui_log = new UILogView(m_ui_main, buf);
	m_win_list.push_back(m_ui_log);

	stbsp_sprintf(buf, u8"%s 关于", ICON_FA_INFO);
	m_win_list.push_back(new UIHelpAbout(m_ui_main, buf));

#ifdef _DEBUG
	stbsp_sprintf(buf, u8"%s 图标", ICON_FA_ICONS);
	m_ui_icons = new UIIconsFontView(m_ui_main, buf);
	m_win_list.push_back(m_ui_icons);
#endif

	stbsp_sprintf(buf, u8"%s Modbus连接", ICON_FA_PLUG);
	m_ui_conn = new UIConnectionSettings(m_ui_main, buf);
	m_win_list.push_back(m_ui_conn);

    stbsp_sprintf(buf, u8"%s 串口日志", ICON_FA_PLUG_CIRCLE_EXCLAMATION);
    m_ui_serial = new UISerialCon(m_ui_main, buf);
    m_win_list.push_back(m_ui_serial);

    stbsp_sprintf(buf, u8"%s ADC串口", ICON_FA_PLUG_CIRCLE_EXCLAMATION);
    m_ui_adc_serial = new UIADCSerialCon(m_ui_main, buf);
    m_win_list.push_back(m_ui_adc_serial);

	stbsp_sprintf(buf, u8"%s 其他串口传感器", ICON_FA_PLUG_CIRCLE_EXCLAMATION);
	m_ui_other_serial = new UIOtherSerialCon(m_ui_main, buf);
	m_win_list.push_back(m_ui_other_serial);

	stbsp_sprintf(buf, u8"%s 电机控制", ICON_FA_MICROCHIP);
	m_ui_motor = new UIMotorControl(m_ui_main, buf);
	m_win_list.push_back(m_ui_motor);

    stbsp_sprintf(buf, u8"%s 采样监控", ICON_FA_TV);
    m_ui_adc = new UIADCMonitor(m_ui_main, buf);
    m_win_list.push_back(m_ui_adc);

	stbsp_sprintf(buf, u8"%s COM监控", ICON_FA_TV);
	m_ui_com = new UICOMMonitor(m_ui_main, buf);
	m_win_list.push_back(m_ui_com);

	stbsp_sprintf(buf, u8"%s 蓝牙监控", ICON_FA_SIGNAL);
	m_ui_ble_monitor = new UIBLEMonitor(m_ui_main, buf);
	m_win_list.push_back(m_ui_ble_monitor);

	stbsp_sprintf(buf, u8"%s Opt采样", ICON_FA_FILE_CSV);
	m_ui_opt_sampling = new UIOptSampling(m_ui_main, buf);
	m_win_list.push_back(m_ui_opt_sampling);

    //stbsp_sprintf(buf, u8"%s Hypersen", ICON_FA_DRUM_STEELPAN);
    //m_ui_hypersen = new UIHypersenMonitor(m_ui_main, buf);
    //m_win_list.push_back(m_ui_hypersen);

    //stbsp_sprintf(buf, u8"%s 夹爪力控", ICON_FA_HANDS_BUBBLES);
    //m_ui_yhgripper = new UIYHGripper(m_ui_main, buf);
    //m_win_list.push_back(m_ui_yhgripper);

    stbsp_sprintf(buf, u8"%s UR机器人", ICON_FA_ROBOT);
    m_ui_urobot = new UIURobot(m_ui_main, buf);
    m_win_list.push_back(m_ui_urobot);

    stbsp_sprintf(buf, u8"%s OptiTrack", ICON_FA_CUBE);
    m_ui_opt = new UIOptiTrack(m_ui_main, buf);
    m_win_list.push_back(m_ui_opt);

    //stbsp_sprintf(buf, u8"%s 管道机器人", ICON_FA_WORM);
    //m_ui_piperobot = new UIPipeRobot(m_ui_main, buf);
    //m_win_list.push_back(m_ui_piperobot);

    //stbsp_sprintf(buf, u8"%s LQY实验", ICON_FA_VIAL);
    //m_ui_lqyexp = new UILQYExp(m_ui_main, buf);
    //m_win_list.push_back(m_ui_lqyexp);

	stbsp_sprintf(buf, u8"%s 弯扭实验", ICON_FA_VIAL);
	m_ui_beamcalibration = new UIBeamCalibration(m_ui_main, buf);
	m_win_list.push_back(m_ui_beamcalibration);
}

void UIWindowManager::InitMenus()
{

	// 创建主菜单
	{
		UIMainMenu m;
		stbsp_sprintf(m.name, u8"%s 系统", ICON_FA_SCREWDRIVER_WRENCH);
		m.cat = EUIMenuCategory::E_UI_CAT_SYS;
		m.show = true;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 编辑", ICON_FA_PEN_TO_SQUARE);
		m.cat = EUIMenuCategory::E_UI_CAT_EDIT;
		m.show = false;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 显示", ICON_FA_EYE);
		m.cat = EUIMenuCategory::E_UI_CAT_VIEW;
		m.show = true;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 应用", ICON_FA_ROCKET);
		m.cat = EUIMenuCategory::E_UI_CAT_APP;
		m.show = true;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 设备", ICON_FA_SERVER);
		m.cat = EUIMenuCategory::E_UI_CAT_SERVICE;
		m.show = false;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 工具", ICON_FA_TOOLBOX);
		m.cat = EUIMenuCategory::E_UI_CAT_TOOL;
		m.show = true;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 窗口", ICON_FA_TABLE_CELLS_LARGE);
		m.cat = EUIMenuCategory::E_UI_CAT_LAYOUT;
		m.show = true;
		m_ui_main->AddMainMenu(m);

		stbsp_sprintf(m.name, u8"%s 帮助", ICON_FA_CIRCLE_QUESTION);
		m.cat = EUIMenuCategory::E_UI_CAT_HELP;
		m.show = true;
		m_ui_main->AddMainMenu(m);
	}

	{
		char buf[256] = { 0 };
		// 1. 窗口菜单前面
		stbsp_sprintf(buf, u8"%s 打开...", ICON_FA_FOLDER_OPEN);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_SYS, [&]() {
			this->ShowOpenModelFileDialog();
		}, buf, "Ctrl+O"));

		stbsp_sprintf(buf, u8"%s 导出...", ICON_FA_FOLDER_OPEN);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_SYS, [&]() {
			this->ShowSaveModelFileDialog();
		}, buf, "Ctrl+P"));
		// 2. 窗口菜单
		std::for_each(m_win_list.begin(), m_win_list.end(), [&](UIBaseWindow* win) {
			UIWindowMenuPtr m = std::make_shared<UIWindowMenu>(win);
			m_ui_main->AddMenuItem(m);
		});

		//3. 窗口菜单之后
		stbsp_sprintf(buf, u8"%s 退出", ICON_FA_RIGHT_FROM_BRACKET);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_SYS, [&]() {
			m_ui_main->SignalClose();
		}, buf, "Ctrl+Q"));

#ifdef _DEBUG
		stbsp_sprintf(buf, u8"%s Demo", ICON_FA_BARS_STAGGERED);
		UISubMenuPtr demo_menu = std::make_shared<UISubMenu>(EUIMenuCategory::E_UI_CAT_VIEW, buf);

		stbsp_sprintf(buf, u8"%s ImGUI Demo", ICON_FA_ICONS);
		demo_menu->AddChildMenu(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_VIEW, [&]() {
			this->SetShowDemo(true);
		}, buf, nullptr));
#ifdef UI_CFG_USE_IMPLOT
		stbsp_sprintf(buf, u8"%s ImPlot Demo", ICON_FA_DRAW_POLYGON);
		demo_menu->AddChildMenu(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_VIEW, [&]() {
			this->SetShowPlotDemo(true);
		}, buf, nullptr));
#endif
		demo_menu->AddChildMenu(std::make_shared<UIWindowMenu>(m_ui_icons));
		m_ui_main->AddMenuItem(demo_menu);
#endif
		stbsp_sprintf(buf, u8"%s 全屏", ICON_FA_EXPAND);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_VIEW, [&]() {
			m_ui_main->ToggleFullscreen();
		}, buf, "Ctrl+Shift+F"));

		stbsp_sprintf(buf, u8"%s 隐藏所有窗口", ICON_FA_WINDOW_MINIMIZE);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_LAYOUT, [&]() {
			this->HideAllWindow();
		}, buf, "Ctrl+Shift+H"));

		stbsp_sprintf(buf, u8"%s 显示所有窗口", ICON_FA_WINDOW_MAXIMIZE);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_LAYOUT, [&]() {
			this->ShowAllWindow();
		}, buf, "Ctrl+Shift+D"));

		stbsp_sprintf(buf, u8"%s 重置窗口布局", ICON_FA_WINDOW_RESTORE);
		m_ui_main->AddMenuItem(std::make_shared<UIFunctionMenu>(
			EUIMenuCategory::E_UI_CAT_LAYOUT, [&]() {
			this->SetResetLayoutFlag();
		}, buf, "Ctrl+Shift+R"));
	}
}

void UIWindowManager::InitImDialog()
{
	// ImGuiFileDialog settings
	// define style by file extention and Add an icon for .png files 
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByExtention, ".jpg", ImVec4(0.0f, 1.0f, 1.0f, 0.9f), ICON_IGFD_FILE);
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByExtention, ".png", ImVec4(0.0f, 1.0f, 0.5f, 0.9f), ICON_IGFD_FILE);
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByExtention, ".bmp", ImVec4(1.0f, 1.0f, 0.5f, 0.9f), ICON_IGFD_FILE);
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByExtention, ".gif", ImVec4(1.0f, 0.5f, 0.5f, 0.9f), ICON_IGFD_FILE);

	// define style for all directories
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByTypeDir, "", ImVec4(0.5f, 1.0f, 0.9f, 0.9f), ICON_IGFD_FOLDER);
	// can be for a specific directory
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByTypeDir, ".git", ImVec4(0.5f, 1.0f, 0.9f, 0.9f), ICON_IGFD_FOLDER);

	// define style for all files
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByTypeFile, "", ImVec4(0.5f, 1.0f, 0.9f, 0.9f), ICON_IGFD_FILE);
	// can be for a specific file
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByTypeFile, ".git", ImVec4(0.5f, 1.0f, 0.9f, 0.9f), ICON_IGFD_FILE);
}

void UIWindowManager::InitShortCut()
{
	m_ui_main->AddShortCut(std::make_shared<UIShortCut>([&]() {
		UI_INFO("Shortcut [Ctrl+i] triggered!");
	}, "test", "Ctrl+I"));
}

void UIWindowManager::ResetLayout()
{
#ifdef IMGUI_HAS_DOCK
	ImGuiID dock_id = m_ui_main->GetRootDockSpaceID();

	ImGui::DockBuilderRemoveNode(dock_id); // Clear out existing layout
	ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
	ImGui::DockBuilderSetNodeSize(dock_id, ImGui::GetMainViewport()->WorkSize);

	ImGuiID dock_main_id = dock_id; // This variable will track the document node, however we are not using it here as we aren't docking anything into it.
	ImGuiID dock_id_log = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, NULL, &dock_main_id);
	ImGuiID dock_id_con = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, NULL, &dock_main_id);

	//ImGuiID dock_id_right_down;
	//ImGuiID dock_id_right_up = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.5f, NULL, &dock_id_right_down);

	// set node local flags
	//ImGuiDockNode* node_log = ImGui::DockBuilderGetNode(dock_id_log);
	//node_log->SetLocalFlags(ImGuiDockNodeFlags_NoTabBar);

	m_ui_conn->Show();
    m_ui_serial->Show();
    m_ui_adc_serial->Show();
	m_ui_log->Show();
	m_ui_motor->Show();
    m_ui_adc->Show();
	m_ui_com->Show();
	m_ui_ble_monitor->Show();
	m_ui_opt_sampling->Show();
	
    //m_ui_hypersen->Show();
    //m_ui_yhgripper->Show();
    m_ui_urobot->Show();
    m_ui_opt->Show();
    //m_ui_piperobot->Show();
    //m_ui_lqyexp->Show();
	m_ui_beamcalibration->Show();

	ImGui::DockBuilderDockWindow(m_ui_conn->GetWinTitle(), dock_id_con);
    ImGui::DockBuilderDockWindow(m_ui_serial->GetWinTitle(), dock_id_con);
    ImGui::DockBuilderDockWindow(m_ui_adc_serial->GetWinTitle(), dock_id_con);
    
	ImGui::DockBuilderDockWindow(m_ui_motor->GetWinTitle(), dock_main_id);
    ImGui::DockBuilderDockWindow(m_ui_adc->GetWinTitle(), dock_main_id);
	ImGui::DockBuilderDockWindow(m_ui_com->GetWinTitle(), dock_main_id);
	ImGui::DockBuilderDockWindow(m_ui_ble_monitor->GetWinTitle(), dock_main_id);
	ImGui::DockBuilderDockWindow(m_ui_opt_sampling->GetWinTitle(), dock_main_id);
    //ImGui::DockBuilderDockWindow(m_ui_hypersen->GetWinTitle(), dock_main_id);
    //ImGui::DockBuilderDockWindow(m_ui_yhgripper->GetWinTitle(), dock_main_id);
    ImGui::DockBuilderDockWindow(m_ui_urobot->GetWinTitle(), dock_main_id);
    ImGui::DockBuilderDockWindow(m_ui_opt->GetWinTitle(), dock_main_id);
    //ImGui::DockBuilderDockWindow(m_ui_piperobot->GetWinTitle(), dock_main_id);
    //ImGui::DockBuilderDockWindow(m_ui_lqyexp->GetWinTitle(), dock_main_id);
	ImGui::DockBuilderDockWindow(m_ui_beamcalibration->GetWinTitle(), dock_main_id);
    
	ImGui::DockBuilderDockWindow(m_ui_log->GetWinTitle(), dock_id_log);

	ImGui::DockBuilderFinish(dock_id);
#endif
}

void UIWindowManager::OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len)
{
	// forward cps message to every ui
	for (size_t i = 0; i < m_win_list.size(); i++)
	{
		m_win_list[i]->OnCPSMsg(from_id, msg_type, data, msg_len);
	}
}

void UIWindowManager::Destroy()
{
	// release window ptr
	for (size_t i = 0; i < m_win_list.size(); i++)
	{
		delete m_win_list[i];
	}
	m_win_list.clear();
}

void UIWindowManager::ShowOpenModelFileDialog()
{
	const char* filters = "All files{.jpg,.png,.bmp,.gif,},.jpg,.png,.bmp,.gif,";
	char buf[256] = { 0 };
	stbsp_sprintf(buf, u8"%s 打开文件", ICON_IGFD_FOLDER_OPEN);
	IGFD::FileDialogConfig config; 
	config.path = ".";
	config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
	ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", buf, filters, config);
}

void UIWindowManager::ShowSaveModelFileDialog()
{
	const char* filters = "All files{.jpg,.png},.jpg,.png";
	char buf[256] = { 0 };
	stbsp_sprintf(buf, u8"%s 导出", ICON_IGFD_SAVE);
	IGFD::FileDialogConfig config;
	config.path = ".";
	config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
	ImGuiFileDialog::Instance()->OpenDialog("SaveFileDlgKey", buf, filters, config);
}

void UIWindowManager::OpenFile(const std::string& filename)
{
	UI_INFO(u8"Opening file %s ...", filename.c_str());
	m_ui_image->ShowImageFromFile(filename.c_str());
}

void UIWindowManager::SaveFile(const std::string& filename)
{
	UI_INFO(u8"export to file %s ...", filename.c_str());
}

void UIWindowManager::ShowAllWindow()
{
	m_show_demo = true;
	m_show_plot_demo = true;

	for (size_t i = 0; i < m_win_list.size(); i++)
	{
		if (m_win_list[i]->GetWinMenuCategory() != EUIMenuCategory::E_UI_CAT_HELP && // 不显示帮助窗口
			m_win_list[i]->GetWinMenuCategory() != EUIMenuCategory::E_UI_CAT_POPUP) // 不显示弹出式窗口
		{
			m_win_list[i]->Show();
		}
	}
}

void UIWindowManager::HideAllWindow()
{
	m_show_demo = false;
	m_show_plot_demo = false;

	for (size_t i = 0; i < m_win_list.size(); i++)
	{
		m_win_list[i]->Hide();
	}
}

void UIWindowManager::ShowDemo()
{
	if (!m_show_demo)
	{
		return;
	}
#ifdef _DEBUG
	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	ImGui::ShowDemoWindow(&m_show_demo);
#endif
}

#ifdef UI_CFG_USE_IMPLOT
void UIWindowManager::ShowPlotDemo()
{
	if (!m_show_plot_demo)
	{
		return;
	}
#ifdef _DEBUG
	ImPlot::ShowDemoWindow(&m_show_plot_demo);
#endif
}
#endif