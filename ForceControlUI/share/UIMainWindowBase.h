#pragma once
#include "UIDef.h"

enum UIMainWindowFlags
{
	UIMAIN_WIN_FLAG_NONE = 0,
	UIMAIN_WIN_FLAG_MAXMIZE = 1,
	UIMAIN_WIN_FLAG_NORMAL_CENTER = 1 << 1,
	UIMAIN_WIN_FLAG_NO_DECORATION = 1 << 2,
	UIMAIN_WIN_FLAG_NO_TASKBARICON = 1 << 3,
	UIMAIN_WIN_FLAG_FLOATING = 1 << 4
};

enum class UIMainWindowState
{
	Normal = 0,
	Maximized = 1,
	Iconified = 2,
	Fullscreen = 3
};

enum class EHWVendorType
{
	E_UNKNOWN = 0,
	E_NVIDIA,
	E_AMD,
	E_INTEL
};

class UIMainWindowBase
{
public:
	UIMainWindowBase();
	virtual ~UIMainWindowBase();
		
	virtual bool IsRunningOnHPG() { 
		return GetVendorType() == EHWVendorType::E_AMD || 
			GetVendorType() == EHWVendorType::E_NVIDIA; }
public: // interface
	virtual bool CreateMainWindow(int width, int height, const char* title, 
		int flags = UIMAIN_WIN_FLAG_NONE, void* parent = nullptr) = 0;
	virtual void DestroyMainWindow() = 0;
	virtual void SetWindowMinSize(int minw, int minh);

	virtual bool SetWindowIconFromFile(const char* iconfile) = 0;
	virtual bool SetWindowIconFromMemoryFile(const unsigned char* icondata, int len) = 0;
	virtual bool LoadTextureFromFile(const char* filename, 
		UI_TEXTURE_ID* out_texture, int* out_width, int* out_height) = 0;
	virtual bool LoadUITextureFromFile(const char* filename, ST_UITexture& texture);
	virtual UI_TEXTURE_ID CreateTexture(unsigned char* in_image_data, int in_width, int in_height, int channels = 4) = 0;
	virtual void DeleteTexture(UI_TEXTURE_ID* texture_id) = 0;
	virtual void UpdateTexture(UI_TEXTURE_ID texture_id, unsigned char* in_image_data, int in_width, int in_height, int channels = 4) = 0;
	virtual void DeleteUITexture(ST_UITexture* texture);

	virtual void* GetNativeHandle() = 0;
	virtual EHWVendorType GetVendorType() = 0;

	virtual void Show() = 0;
	virtual void Hide() = 0;
	virtual bool IsVisible() = 0;
	virtual void Iconify() = 0;
	virtual void Maximize() = 0;
	virtual void Restore() = 0;
	virtual void ToggleFullscreen() = 0;
	virtual bool IsFullScreen() = 0;

	virtual const char* GetWindowTitle() = 0;
	virtual float GetTitleBarHeight() = 0;
	virtual bool IsBoarderLess() = 0;
	virtual float GetCurMonitorScale() = 0;

	virtual UIMainWindowState GetWindowState() = 0;

	virtual void RefreshWindow() = 0;

	virtual void SetVSync(bool vsync) = 0;
	virtual void SwapBuffers() = 0;

	virtual void SignalClose(int value = 1) = 0;
	virtual int WindowShouldClose() = 0;

	virtual void PollMessages() = 0;
	virtual void WaitMessages() = 0;
	virtual void WaitMessagesTimeout(double timeout_sec) = 0;
public: // render interface
	virtual void FrameInit() = 0;
	//! FrameStart
	virtual void FrameStart() {}
	//! FrameEnd
	virtual void FrameEnd() {}
	virtual void SetBgColor(float r, float g, float b, float a) = 0;
	virtual void GetRenderSize(int& width, int& height) = 0;
	virtual void FrameDestroy() {};
public: // interface
	//! FrameUpdate
	virtual void FrameUpdate() {}
	//! Window close
	virtual void OnWindowClose() {}
	//! Window resize event.
	virtual void OnWindowResize(int theWidth, int theHeight) {}
	//! Window pos event
	virtual void OnWindowPos(int xpos, int ypos) {}
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
	virtual void OnContentScale(float xscale, float yscale) {}
protected:
	// 窗口最小值，不允许比这个值更小,borderless 模式下有效
	int m_min_limit_window_size[2] = { 640, 480 };
};

