#include "UISysSettings.h"
#include <cmath>
#include "UIMainWindow.h"
#include "stb/stb_sprintf.h"
#include "IconsFontAwesome6.h"
#include "StrTool.h"
#include "UILog.h"
#include "UIFontLoader.h"
#include <algorithm>
#include "UIUtils.h"

UISysSettings::UISysSettings(UIMainWindowBase* main_win, const char* title) :UIBaseWindow(main_win, title)
{
    // 初始化主题名称列表
    m_main_win = dynamic_cast<UIMainWindow*>(main_win);
    m_vec_theme_name = m_main_win->GetStyleNameList();
    std::string cur_theme = m_main_win->CurrentStyleName();
    size_t idx = 0;
    for (size_t i = 0; i < m_vec_theme_name.size(); i++)
    {
		if (cur_theme == m_vec_theme_name[i])
		{
			m_style_idx = i;
		}
        memcpy(m_theme_name_join + idx, m_vec_theme_name[i].c_str(), m_vec_theme_name[i].size());
        idx += m_vec_theme_name[i].size();
        m_theme_name_join[idx] = '\0';
        idx += 1;
    }
        
    // 默认字体
    const UIFontProp * default_font = UIFontLoader::Inst()->GetMainFrameFont();
    m_cur_main_font_size = default_font->font_size;
    m_cur_main_frame_font_idx = 0;

    // 初始化字体列表
    m_vec_font_files.push_back(UIFontFile(default_font->font_name, default_font->font_file_path)); // 默认由UIFontLoader加载，无需设置
    m_vec_font_files.push_back(UIFontFile(u8"宋体", "C:/Windows/Fonts/simsun.ttc"));
    m_vec_font_files.push_back(UIFontFile(u8"仿宋", "C:/Windows/Fonts/simfang.ttf"));
    m_vec_font_files.push_back(UIFontFile(u8"微软雅黑", "C:/Windows/Fonts/msyh.ttc"));
    m_vec_font_files.push_back(UIFontFile(u8"楷体", "C:/Windows/Fonts/simkai.ttf"));
    m_vec_font_files.push_back(UIFontFile(u8"幼圆", "C:/Windows/Fonts/simyou.ttf"));
    m_vec_font_files.push_back(UIFontFile(u8"隶书", "C:/Windows/Fonts/simli.ttf"));
    m_vec_font_files.push_back(UIFontFile(u8"方正舒体", "C:/Windows/Fonts/FZSTK.ttf"));
    m_vec_font_files.push_back(UIFontFile(u8"方正姚体", "C:/Windows/Fonts/FZYTK.ttf"));
    m_vec_font_files.push_back(UIFontFile(u8"不存在字体", "C:/Windows/Fonts/xxxx.ttf"));
    // 删除不存在的字体
    m_vec_font_files.erase(std::remove_if(m_vec_font_files.begin(),
        m_vec_font_files.end(), [](const UIFontFile& item) {
            return !UIUtils::Inst()->IsFileExists(item.font_file_path);
        }), m_vec_font_files.end());
}

void UISysSettings::Draw()
{
    if (!m_show)
    {
        return;
    }

    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
	char buf[64] = { 0 };
    if (ImGui::CollapsingHeader(u8"字体设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        stbsp_sprintf(buf, u8"%s 选择字体", ICON_FA_FONT);
        ImGui::Text(buf);
        const char* combo_preview_value = m_cur_main_frame_font_idx >= 0 ? m_vec_font_files[m_cur_main_frame_font_idx].font_name.c_str() : "";  // Pass in the preview value visible before opening the combo (it could be anything)
        if (ImGui::BeginCombo(u8"选择字体", combo_preview_value, 0))
        {
            for (size_t n = 0; n < m_vec_font_files.size(); n++)
            {
                const bool is_selected = (m_cur_main_frame_font_idx == n);
                if (ImGui::Selectable(m_vec_font_files[n].font_name.c_str(), is_selected))
                {
                    m_cur_main_frame_font_idx = n;

                    UIFontLoader::Inst()->SetMainFrameFont(m_vec_font_files[m_cur_main_frame_font_idx].font_file_path,
                        m_vec_font_files[m_cur_main_frame_font_idx].font_name, m_cur_main_font_size);
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        stbsp_sprintf(buf, u8"%s 字体大小", ICON_FA_ARROW_UP_1_9);
        ImGui::Text(buf);
        if (ImGui::InputFloat(u8"字体大小", &m_cur_main_font_size, 1, 2, "%.0f", ImGuiInputTextFlags_None))
        {
            //clamp to valid range
            m_cur_main_font_size = m_cur_main_font_size < 12 ? 12 : m_cur_main_font_size;
            m_cur_main_font_size = m_cur_main_font_size > 30 ? 30 : m_cur_main_font_size;
            UIFontLoader::Inst()->SetMainFrameFont(m_vec_font_files[m_cur_main_frame_font_idx].font_file_path,
                m_vec_font_files[m_cur_main_frame_font_idx].font_name, m_cur_main_font_size);
        }
    }
    if (ImGui::CollapsingHeader(u8"窗口设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        stbsp_sprintf(buf, u8"%s 设置窗口背景色", ICON_FA_PALETTE);
        ImGui::Text(buf);
        UIStyle *uistyle = m_main_win->CurrentStyle();
        
        if (ImGui::ColorEdit3(u8"背景色", (float*)&uistyle->bk_color1)) // Edit 3 floats representing a color
        {
        }

        stbsp_sprintf(buf, u8"%s 选择界面主题", ICON_FA_COOKIE);
        ImGui::Text(buf);
        if (ImGui::Combo(u8"主题", &m_style_idx, m_theme_name_join))
        {
            m_main_win->SetUIStyle(m_vec_theme_name[m_style_idx]);
        }
        stbsp_sprintf(buf, u8"%s 设置界面透明度", ICON_FA_CIRCLE_HALF_STROKE);
        ImGui::Text(buf);
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::SliderFloat(u8"透明度", &style.Colors[ImGuiCol_WindowBg].w, 0.0f, 1.0f, "%.1f");

        stbsp_sprintf(buf, u8"%s 设置UI字体缩放", ICON_FA_LEFT_RIGHT);
        ImGui::Text(buf);
        if (ImGui::SliderFloat(u8"字体缩放", &ImGui::GetIO().FontGlobalScale, 1.0f, 3.0f, "%.1f"))  // Edit 1 float using a slider from 0.0f to 1.0f
        {
        }

        stbsp_sprintf(buf, u8"%s 设置菜单显示", ICON_FA_BARS);
        ImGui::Text(buf);
        bool show_menu = m_main_win->IsMainMenuVisible();
        if (ImGui::Checkbox(u8"显示主菜单", &show_menu))
        {
            m_main_win->SetMainMenuVisible(show_menu);
        }

        ImGui::SameLine();
		show_menu = m_main_win->IsStatusBarVisible();
		if (ImGui::Checkbox(u8"显示状态栏", &show_menu))
		{
			m_main_win->SetStatusBarVisible(show_menu);
		}

        ImGui::SameLine();
        show_menu = m_main_win->IsBannerLogoWindowVisible();
        if (ImGui::Checkbox(u8"显示LOGO", &show_menu))
        {
            m_main_win->ShowBannerLogoWindow(show_menu);
        }
    }
    if (ImGui::CollapsingHeader(u8"日志设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
		stbsp_sprintf(buf, u8"%s 设置UI日志等级", ICON_FA_LIST);
		ImGui::Text(buf);
		int log_level = (int)UILog::Inst()->GetShowLogLevel();
		if (ImGui::Combo(u8"UI日志等级", &log_level, u8"调试\0信息\0警告\0错误\0致命\0"))
		{
			UILog::Inst()->SetShowLogLevel((E_UI_LOG_LEVEL)log_level);
		}
        bool enable_file_log = UILog::Inst()->IsFileLogEnabled();
		if (ImGui::Checkbox(u8"启用文件日志", &enable_file_log))
		{
            UILog::Inst()->EnableFileLog(enable_file_log);
		}
        if (enable_file_log)
        {
            stbsp_sprintf(buf, u8"%s 设置文件日志等级", ICON_FA_FILE_LINES);
            ImGui::Text(buf);
            log_level = (int)UILog::Inst()->GetFileLogLevel();
            if (ImGui::Combo(u8"文件日志等级", &log_level, u8"调试\0信息\0警告\0错误\0致命\0"))
            {
                UILog::Inst()->SetFileLogLevel((E_UI_LOG_LEVEL)log_level);
            }
        }
    }
    if (ImGui::CollapsingHeader(u8"高级设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
		stbsp_sprintf(buf, u8"%s 设置OpenGL模式", ICON_FA_SLIDERS);
		ImGui::Text(buf);
        int mode = (int)m_main_win->GetUIMode();
		if (ImGui::Combo(u8"模式", &mode, u8"性能模式\0平衡模式\0固定模式\0"))
		{
			switch (mode)
			{
			case 0: m_main_win->SetUIMode(E_UI_POLL_MODE); break;
			case 1: m_main_win->SetUIMode(E_UI_WAIT_MODE); break;
			case 2: m_main_win->SetUIMode(E_UI_WAIT_TIMEOUT_MODE); break;
			}
		}
        if (mode == int(E_UI_WAIT_TIMEOUT_MODE))
        {
            float timeout = (float)m_main_win->GetUIRefreshTimeout();
            int hz = (int)round(1.0f / timeout);
            if (ImGui::SliderInt(u8"刷新率(HZ)", &hz, 1, 1000))
            {
                m_main_win->SetUIRefreshTimeout(1.0/hz);
            }
        }
        if (ImGui::Checkbox(u8"开启VSYNC", &m_vsync))
        {
            m_main_win->SetVSync(m_vsync);
        }
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(246/255.0f, 160/255.0f, 0.0, 1.0), u8"FPS %.3f ms/frame (%.1f FPS)", 
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}
