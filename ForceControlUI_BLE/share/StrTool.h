#pragma once
#include <iostream>
#include <string>

inline bool IsEndsWith(std::string const& fullString, std::string const& ending) 
{
	if (fullString.length() >= ending.length()) 
	{
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	}
	return false;
}

inline bool IsStartsWith(std::string const& fullString, std::string const& start)
{
	if (fullString.length() >= start.length())
	{
		return (0 == fullString.compare(0, start.length(), start));
	}
	return false;
}


template <class T>
std::string JoinStr(T& val, std::string delim)
{
	std::string str;
	typename T::iterator it;
	const typename T::iterator itlast = val.end() - 1;
	for (it = val.begin(); it != val.end(); it++)
	{
		str += *it;
		if (it != itlast)
		{
			str += delim;
		}
	}
	return str;
}

class StrTool
{
public:
	StrTool() = delete;
	StrTool(const StrTool&) = delete;
	StrTool(StrTool&&) = delete;
	~StrTool() = delete;
	StrTool& operator=(const StrTool&) = delete;
	StrTool& operator=(StrTool&&) = delete;

	static std::string    ToString(const std::wstring& wstr);
	static std::wstring   ToWString(const std::string& str);
	static std::string    GBKToUTF8(const std::string& str);
	static std::string    UTF8ToGBK(const std::string& str);
	static std::u16string UTF8toUTF16(const std::string& str);
	static std::u32string UTF8toUTF32(const std::string& str);
	static std::string    UTF16toUTF8(const std::u16string& str);
	static std::u32string UTF16toUTF32(const std::u16string& str);
	static std::string    UTF32toUTF8(const std::u32string& str);
	static std::u16string UTF32toUTF16(const std::u32string& str);
private:
	static std::string    ToGBK(const std::wstring& wstr);
	static std::wstring   FromGBK(const std::string& str);
	static std::string    ToUTF8(const std::wstring& wstr);
	static std::wstring   FromUTF8(const std::string& str);
private:
	static const char* GBK_LOCALE_NAME;
};


#define U2G(s) StrTool::UTF8ToGBK(s)
#define U2GC(s) StrTool::UTF8ToGBK(s).c_str()

#define G2U(s) StrTool::GBKToUTF8(s)
#define G2UC(s) StrTool::GBKToUTF8(s).c_str()
