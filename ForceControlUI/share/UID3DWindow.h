#pragma once
#include "UIMainWindowBase.h"
#ifdef UI_RENDER_ENGINE_D3D11
class UID3DWindow :public UIMainWindowBase
{
public:
	UID3DWindow();
	~UID3DWindow();

	virtual bool CreateMainWindow(int width, int height, const char* title,
		int flags = UIMAIN_WIN_FLAG_NONE, void* parent = nullptr);
	virtual void DestroyMainWindow();

	virtual bool SetWindowIconFromFile(const char* iconfile);
	virtual bool SetWindowIconFromMemoryFile(const unsigned char* icondata, int len);
	virtual bool LoadTextureFromFile(const char* filename,
		UI_TEXTURE_ID* out_texture, int* out_width, int* out_height);
	virtual UI_TEXTURE_ID CreateTexture(unsigned char* in_image_data, int in_width, int in_height, int channels = 4);
	virtual void DeleteTexture(UI_TEXTURE_ID* texture_id);
	virtual void UpdateTexture(UI_TEXTURE_ID texture_id, unsigned char* in_image_data, int in_width, int in_height, int channels = 4);

	// call after CreateWindow
	virtual void* GetNativeHandle();
	virtual EHWVendorType GetVendorType();

	virtual void Show();
	virtual void Hide();
	virtual bool IsVisible();
	virtual void Iconify();
	virtual void Maximize();
	virtual void Restore();
	virtual void ToggleFullscreen();
	virtual bool IsFullScreen();

	virtual const char* GetWindowTitle();
	virtual bool IsBoarderLess();
	virtual float GetCurMonitorScale();

	virtual UIMainWindowState GetWindowState() { return m_window_state; }

	virtual void RefreshWindow();

	virtual void SetVSync(bool vsync);
	virtual void SwapBuffers();

	virtual void SignalClose(int value = 1);
	virtual int WindowShouldClose() { return m_should_close; }

	virtual void PollMessages();
	virtual void WaitMessages();
	virtual void WaitMessagesTimeout(double timeout_sec);
public: // render interface
	void HandleResize();
	virtual void FrameInit();
	//! FrameStart
	virtual void FrameStart();
	//! FrameEnd
	virtual void FrameEnd();
	virtual void SetBgColor(float r, float g, float b, float a);
	virtual void GetRenderSize(int& width, int& height);
	virtual void FrameDestroy();
public:
	//! Window resize event.
	virtual void OnWindowResize(int theWidth, int theHeight);
protected: // D3D vars
	friend struct D3DCtx;
	D3DCtx* m_d3d_ctx = nullptr;
protected:
	char m_window_title[256] = { 0 };
	int m_last_pos[2] = { 0, 0 };
	int m_last_size[2] = { 800, 600 };

	int m_should_close = 0;

	UIMainWindowState m_window_state = UIMainWindowState::Normal;
	int m_window_flags = UIMAIN_WIN_FLAG_NONE;	
};

#endif