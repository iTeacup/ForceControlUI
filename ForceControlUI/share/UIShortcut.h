#pragma once
#include <memory>

// ·Ç²Ëµ¥¿ì½Ý¼ü
class UIShortCut
{
public:
	UIShortCut(std::function<void()> func,
		const char* name, const char* shortcut, bool enabled = true):
		m_func(func), m_name(name), m_shorcut(shortcut),m_enabled(enabled)
	{

	}

	void SetShortcut(const char* shortcut) { m_shorcut = shortcut; }
	const std::string& GetShortcut() { return m_shorcut; }

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	bool GetEnabled() { return m_enabled; }

	const std::string& GetName() { return m_name; }

	void OnKeyEvent(const char* key)
	{
		if (!m_shorcut.empty() && m_shorcut == std::string(key))
		{
			m_func();
		}
	}
protected:
	std::function<void()> m_func;
	std::string m_name;
	std::string m_shorcut;
	bool m_enabled;
};

using UIShortCutPtr = std::shared_ptr<UIShortCut>;
