#pragma once
#include <future>
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"

// 字体属性
struct UIFontProp
{
	std::string font_name = "";
	std::string font_file_path = "";

	ImFont* font = nullptr;
	float font_size = 16.0f;
};

class UIFontLoader
{
public:
	static UIFontLoader* Inst()
	{
		static UIFontLoader loader;
		return &loader;
	}
	void SetExtraChars(const ImWchar* data, size_t size);
	void LoadFont();

	ImFontAtlas* GetFontAtlas();

	UIFontProp* SetMainFrameFont(const std::string filename, const std::string& font_name, float font_size);
	UIFontProp* GetMainFrameFont();
	bool IsMainFontChanged();

	ImFont* GetFontH1() { return m_font_h1; }
	ImFont* GetFontH2() { return m_font_h2; }
	ImFont* GetFontH3() { return m_font_h3; }

	ImFont* LoadFontWithSize(const char* filename, float size_pixels);
protected:
	UIFontLoader();
	~UIFontLoader();

	void _DoLoad();
	void LoadDefaultFont();
protected:
	std::vector<ImWchar> m_extra_chars;
	std::future<void> m_load_font_finish;
	ImFontAtlas* m_font_atlas = nullptr;
	ImVector<ImWchar> m_out_ranges; // 常用字以及额外定义字范围

	// 主界面字体
	UIFontProp m_main_frame_font;

	ImFont* m_font_h1 = nullptr;
	ImFont* m_font_h2 = nullptr;
	ImFont* m_font_h3 = nullptr;
};
