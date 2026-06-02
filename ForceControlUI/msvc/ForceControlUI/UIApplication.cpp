#include "UIApplication.h"
#include "UIFontLoader.h"
#include "UITextureLoader.h"
#include "TimeCounter.h"
#ifdef UI_CFG_ENABLE_CPS
#include "CPSHandler.h"
#endif
#include "UICfgParser.h"
#include "Logger.h"
#include "UICharDef.h"
#include "UIWindowManager.h"
#include "Title.h"
#include "UIImageDef.h"

UIApplication g_app;

UIApplication::UIApplication()
{

}

UIApplication::~UIApplication()
{

}

void UIApplication::Run()
{
	TimeCounter t;
	// async load font first
	UIFontLoader::Inst()->SetExtraChars(EXTRA_IMCHAR_LIST, sizeof(EXTRA_IMCHAR_LIST) / sizeof(EXTRA_IMCHAR_LIST[0]));
	UIFontLoader::Inst()->LoadFont();
	// add custom images
	UITextureLoader::Inst()->AddImageFile(APP_MY_IMAGE_ID, APP_MY_IMAGE_FILE);
	// overwrite logo, icon, logo banner image
	UITextureLoader::Inst()->AddImageFile((int)E_UI_TEXTURE_APP_ICON, APP_ICON_IMAGE_FILE);
	UITextureLoader::Inst()->AddImageFile((int)E_UI_TEXTURE_APP_LOGO, APP_LOGO_IMAGE_FILE);
	UITextureLoader::Inst()->AddImageFile((int)E_UI_TEXTURE_APP_LOGO_BANNER, APP_LOGO_BANNER_IMAGE_FILE);
	// async load images
	UITextureLoader::Inst()->LoadAllUIImages();
	// 1. init cfg
	g_cfg->LoadCfg();
	// 2. init cps
#ifdef UI_CFG_ENABLE_CPS
	InitCPS();
#endif

	// 3. init imgui
	m_main_ui = new UIMainWindow();
	g_main_win = m_main_ui;
	auto win_mng = std::make_shared<UIWindowManager>(m_main_ui);
	m_main_ui->SetWindowManager(win_mng);
#ifdef _WIN32
	m_main_ui->Init(UI_WIN_TITLE, UIMAIN_WIN_FLAG_MAXMIZE | UIMAIN_WIN_FLAG_NO_DECORATION);
#else
	m_main_ui->Init(UI_WIN_TITLE, UIMAIN_WIN_FLAG_NORMAL_CENTER);
#endif
	// overwrite tray window icon
	m_main_ui->SetWindowIconFromFile(APP_ICON_IMAGE_FILE);
	
	// 等待材质加载完毕
	UITextureLoader::Inst()->WaitForReady();

	t.Tick(__FUNCTION__);
	// 4. loop
	m_main_ui->Loop();
	// exit
	Exit();
}

#ifdef UI_CFG_ENABLE_CPS
void UIApplication::OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char* data, uint32_t msg_len)
{
	// forward to ui
	if (m_main_ui && m_main_ui->IsInitialized())
	{
		m_main_ui->OnCPSMsg(from_id, msg_type, data, msg_len);
	}
}

void UIApplication::InitCPS()
{
	TimeCounter t;
	if (!m_cpsapi)
	{
		// create api
		m_cpsapi = CCPSAPI::CreateAPI();
		// create spi
		m_handler = new CPSHandler();

		if (m_cpsapi->Init(E_CPS_TYPE_APP, 0, g_cfg->m_app_id,
			g_cfg->m_cpscfg.bus.ip, g_cfg->m_cpscfg.bus.port,
			g_cfg->m_cpscfg.log.ip, g_cfg->m_cpscfg.log.port,
			m_handler) != 0)
		{
			LOG_ERROR("CPS API init failed!");
			throw std::runtime_error("CPS API init failed!");
			return;
		}
		LOG_DEBUG("CPSAPI Version: %s", m_cpsapi->GetApiVersion());
	}
	t.Tick(__FUNCTION__);
}
#endif

void UIApplication::Exit()
{
	// 隐藏窗口，后台退出
	if (m_main_ui)
	{
		m_main_ui->Hide();
	}
#ifdef UI_CFG_ENABLE_CPS
	// 退出CPS API
	if (m_cpsapi)
	{
		m_cpsapi->Release();
		m_cpsapi = nullptr;
	}
	if (m_handler)
	{
		delete m_handler;
		m_handler = nullptr;
	}
#endif
	UITextureLoader::Inst()->DeleteTextures();
	// imgui Cleanup
	if (m_main_ui)
	{
		m_main_ui->Destroy();
		delete m_main_ui;
		m_main_ui = nullptr;
	}
}
