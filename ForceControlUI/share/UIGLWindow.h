#pragma once
#include "UIMainWindowBase.h"
#ifdef UI_RENDER_ENGINE_OPENGL
struct GLFWwindow;
struct GLFWmonitor;
struct GLFWvidmode;

class UIGLWindow: public UIMainWindowBase
{
public:
	UIGLWindow();
	virtual ~UIGLWindow();

	virtual bool CreateMainWindow(int width, int height, const char * title, 
		int flags = UIMAIN_WIN_FLAG_NONE, void* parent = nullptr);
	virtual void DestroyMainWindow();
	virtual void SetWindowMinSize(int minw, int minh);

	virtual bool SetWindowIconFromFile(const char* iconfile);
	virtual bool SetWindowIconFromMemoryFile(const unsigned char* icondata, int len);
	// must be called from the main thread after glfwInit
	virtual bool LoadTextureFromFile(const char* filename,
		UI_TEXTURE_ID* out_texture, int* out_width, int* out_height);
	virtual UI_TEXTURE_ID CreateTexture(unsigned char* in_image_data, int in_width, int in_height, int channels = 4);
	virtual void DeleteTexture(UI_TEXTURE_ID* texture_id);
	virtual void UpdateTexture(UI_TEXTURE_ID texture_id, unsigned char* in_image_data, int in_width, int in_height, int channels = 4);

	virtual void* GetNativeHandle();
	virtual EHWVendorType GetVendorType() { return m_vendor; }

	virtual void Show();
	virtual void Hide();
	virtual bool IsVisible();
	virtual void Iconify();
	virtual void Maximize();
	virtual void Restore();
	virtual void ToggleFullscreen();
	virtual bool IsFullScreen();

	virtual const char* GetWindowTitle() { return m_window_title; }
	virtual bool IsBoarderLess() { return m_window_flags & UIMAIN_WIN_FLAG_NO_DECORATION; }
	virtual float GetCurMonitorScale() { return m_monitor_xscale; }

	virtual UIMainWindowState GetWindowState() { return m_window_state; }
	
	virtual void RefreshWindow();

	virtual void SetVSync(bool vsync);
	virtual void SwapBuffers();

	virtual void SignalClose(int value = 1);
	virtual int WindowShouldClose();

	virtual void PollMessages();
	virtual void WaitMessages();
	virtual void WaitMessagesTimeout(double timeout_sec);
public: // render interface
	virtual void FrameInit();
	//! FrameStart
	virtual void FrameStart();
	//! FrameEnd
	virtual void FrameEnd();
	virtual void SetBgColor(float r, float g, float b, float a);
	virtual void GetRenderSize(int& width, int& height);
	virtual void FrameDestroy();
public: // custom functions
	static UIGLWindow* GetCurrentWindowPtr();
public: // interface
	//! FrameUpdate
	virtual void FrameUpdate() {}
	//! Window close
	virtual void OnWindowClose() {}
	//! Window resize event.
	virtual void OnWindowResize(int theWidth, int theHeight);
	//! Window pos event
	virtual void OnWindowPos(int xpos, int ypos);
	//! Mouse scroll event.
	virtual void OnMouseScroll(double theOffsetX, double theOffsetY) {}
	//! Mouse click event.
	virtual void OnMouseButton(int theButton, int theAction, int theMods) {}
	//! Mouse move event.
	virtual void OnMouseMove(int thePosX, int thePosY) {}
	//! Mouse enter/leave event
	virtual void OnMouseEnter(int entered) {}
	//! Key callback
	virtual void OnKey(int key, int scancode, int action, int mods) {}
	//! Content scale callback
	virtual void OnContentScale(float xscale, float yscale);
protected:
	//！Error callback
	static void OnError(int theError, const char* theDescription);
protected:
	// returns the monitor that contains the greater window area, or NULL if no monitor found
	GLFWmonitor* GetCurrentMonitor();
protected:
	GLFWwindow* m_glfw_parent_window = nullptr;
	GLFWwindow* m_glfw_window = nullptr;
	GLFWmonitor* m_glfw_primary_monitor = nullptr;
	const GLFWvidmode* m_glfw_primary_video_mode = nullptr;
	char m_window_title[256] = { 0 };
	bool m_vsync = true;

	EHWVendorType m_vendor = EHWVendorType::E_UNKNOWN;
	
	float m_monitor_xscale = 1.0;
	float m_monitor_yscale = 1.0;

	int m_cur_pos[2] = { 0 };
	int m_cur_size[2] = { 0 };

	int m_last_pos[2] = { 0, 0 };
	int m_last_size[2] = { 800, 600 };
	// 主屏幕窗口最大值
	int m_maximum_size[2] = { 0,0 };

	UIMainWindowState m_window_state = UIMainWindowState::Normal;
	int m_window_flags = UIMAIN_WIN_FLAG_NONE;
};

#endif
