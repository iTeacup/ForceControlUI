#pragma once
#include <chrono>
#include <future>
#include <map>
#include <vector>
#include <string>
#include "UIDef.h"

typedef enum
{
	E_UI_TEXTURE_NONE,
	E_UI_TEXTURE_APP_ICON,
	E_UI_TEXTURE_APP_LOGO,
	E_UI_TEXTURE_APP_LOGO_BANNER,
	E_UI_TEXTURE_ID_END
}E_UI_TEXTURE_TYPE;

// 用户自定义图片ID起始值
constexpr unsigned int USER_IMAGE_ID_START = (int)E_UI_TEXTURE_ID_END + 1;

// async texture loader
class UITextureLoader
{
public:
	static UITextureLoader* Inst()
	{
		static UITextureLoader loader;
		return &loader;
	}
	
	void AddImageFile(unsigned int id, const std::string& filename);
	void LoadAllUIImages();

	void WaitForReady();

	ST_UITexture* GetTexture(unsigned int id);

	void DeleteTextures();
protected:
	UITextureLoader();
	~UITextureLoader();
	
	void _DoLoad();
protected:
	std::map<unsigned int, std::string> m_map_image_files;
	std::future<void> m_load_finish;
	std::map<unsigned int, ST_UITexture> m_map_texture;
};