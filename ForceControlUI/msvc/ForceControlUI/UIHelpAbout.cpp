#include "UIHelpAbout.h"
#include "UIDef.h"
#include "IconsFontAwesome6.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "UITextureLoader.h"
#include "Title.h"
#include "UIMainWindow.h"
#include "StrTool.h"


UIHelpAbout::UIHelpAbout(UIMainWindowBase* main_win, const char* title) :UIBaseWindow(main_win, title)
{
    m_main_win = dynamic_cast<UIMainWindow*>(main_win);
}

void UIHelpAbout::Draw()
{
    if (!m_show)
    {
        return;
    }
    //
    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    {
        ImGui::End();
        return;
    }

	ST_UITexture* image_logo = UITextureLoader::Inst()->GetTexture(E_UI_TEXTURE_APP_LOGO);
	if (image_logo)
	{
        // 图像大小固定分辨率
		float width = 256;
        float ratio = width / image_logo->image_width;
        float height = ratio * image_logo->image_height;
		ImGui::Image((intptr_t)image_logo->image_texture_id, ImVec2(width, height));
	}
    ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), u8"软件版本：%s", VERSION_INFO_STR);
    ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), u8"编译时间：%s", VERSION_BUILD_TIME);
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), u8"科技改变世界!");
    ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), u8"Copyright %s 2021 %s. All Rights Reserved", ICON_FA_COPYRIGHT, G2UC(APP_COMPANY_NAME));

    if (ImGui::IsWindowFocused())
    {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_show = false;
        }
    }
    else
    {
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }
    ImGui::End();
}
