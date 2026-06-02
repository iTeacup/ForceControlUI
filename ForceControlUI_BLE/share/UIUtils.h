#pragma once
#include <string>
#include <chrono>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "UIDef.h"

enum EMessageBtnType
{
	E_BTN_OK = 1,
	E_BTN_CANCEL = 1 << 1,
	E_BTN_ALL = E_BTN_OK | E_BTN_CANCEL
};

class UIUtils
{
public:
	static UIUtils* Inst()
	{
		static UIUtils util;
		return &util;
	}
	// must be called from the main thread
	bool InitGlad();

	bool LoadImageFromFile(const char* filename, unsigned char ** out_image_data, int* out_width, int* out_height);
	bool LoadImageFromMemory(const unsigned char* data, int len, unsigned char** out_image_data, int* out_width, int* out_height);
	void FreeImageData(unsigned char*& in_image_data);

	int ShowMessageBox(const char* title, const char* content, EMessageBtnType btn = E_BTN_ALL);
	int ShowProgressDialog(const char* title, const char* content, float fraction, const char* overlay = NULL);

	bool ReadTxtFile(const std::string& filename, std::string& content);
	bool IsFileExists(const std::string& filename);
protected:
	UIUtils();
	~UIUtils();

	bool m_is_glad_init = false;
};

// 简单的Timer类，只可用于MainLoop中
class UITimer
{
public:
	UITimer();
	void StartTimer(double interval_sec);
	void StopTimer();
	bool CheckTimer();
protected:
	// a simple timer
	double m_last_check_time_sec = 0;
	bool m_timer_enable = false;
	double m_timer_interval_sec = 0.0f;
};


static int FastSecondToDate(const time_t& unix_sec, struct tm* tm, int time_zone)
{
	static const int kHoursInDay = 24;
	static const int kMinutesInHour = 60;
	static const int kDaysFromUnixTime = 2472632;
	static const int kDaysFromYear = 153;
	static const int kMagicUnkonwnFirst = 146097;
	static const int kMagicUnkonwnSec = 1461;
	tm->tm_sec = int(unix_sec % kMinutesInHour);
	int i = int(unix_sec / kMinutesInHour);
	tm->tm_min = i % kMinutesInHour; //nn
	i /= kMinutesInHour;
	tm->tm_hour = (i + time_zone) % kHoursInDay; // hh
	tm->tm_mday = (i + time_zone) / kHoursInDay;
	int a = tm->tm_mday + kDaysFromUnixTime;
	int b = (a * 4 + 3) / kMagicUnkonwnFirst;
	int c = (-b * kMagicUnkonwnFirst) / 4 + a;
	int d = ((c * 4 + 3) / kMagicUnkonwnSec);
	int e = -d * kMagicUnkonwnSec;
	e = e / 4 + c;
	int m = (5 * e + 2) / kDaysFromYear;
	tm->tm_mday = -(kDaysFromYear * m + 2) / 5 + e + 1;
	tm->tm_mon = (-m / 10) * 12 + m + 2;
	tm->tm_year = b * 100 + d - 6700 + (m / 10);
	return 0;
}

template <typename T, typename Func>
bool custom_combo(const std::vector<T> &items, const char *label, char *buf, int &selected_index, Func preview_func)
{
    if (selected_index >= 0 && selected_index < items.size())
    {
        preview_func(items[selected_index], selected_index, buf);
    }
    else
    {
        buf[0] = '\0'; // Reset buffer if no valid selection
    }
    bool clicked = false;
    // Check if the combo box is clicked
    if (ImGui::BeginCombo(label, buf))
    {
        for (size_t n = 0; n < items.size(); n++)
        {
            const T &item = items[n];
            // Use the provided function to generate the item label
            preview_func(item, n, buf);
            bool is_selected = (selected_index == static_cast<int>(n));
            if (ImGui::Selectable(buf, is_selected))
            {
                selected_index = static_cast<int>(n);
                clicked = true; // Mark that an item was clicked
            }
            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return clicked;
}
