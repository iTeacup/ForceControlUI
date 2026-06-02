#include "UIPipeRobot.h"
#include "UIApplication.h"
#include "stb/stb_sprintf.h"

UIPipeRobot::UIPipeRobot(UIMainWindowBase *main_win, const char *title): UIBaseWindow(main_win, title)
{
    m_cpsapi = g_app.GetCPSApi();
}

UIPipeRobot::~UIPipeRobot()
{
}

void UIPipeRobot::Draw()
{
    if (!m_show)
    {
        return;
    }
    char buf[64] = {0};
    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    bool is_device_online = m_cpsapi->IsDeviceOnline(PIPE_ROBOT_DEV_ID);
    stbsp_sprintf(buf, "%s", is_device_online ? u8"在线" : u8"离线");
    ImGui::InputText(u8"机器人状态", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);


    
    ImGui::End();
}

void UIPipeRobot::OnCPSMsg(uint32_t from_id, uint32_t msg_type, const char *data, uint32_t msg_len)
{
    // 仅处理来自PipeRobot的消息
    if (from_id != PIPE_ROBOT_DEV_ID)
    {
        return;
    }
    switch (msg_type)
    {
    default:
        break;
    }
}
