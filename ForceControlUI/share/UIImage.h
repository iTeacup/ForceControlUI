#pragma once
#include <memory>
#include "UIDef.h"
#ifdef UI_CFG_USE_OPENCV
#include <opencv2/opencv.hpp>
#endif

enum class UI_IMAGE_RESIZE_POLICY
{
	NORMAL,
	KEEP_ASPECT
};

// base class for UIImage
class UIImageBase
{
public:
	virtual bool LoadFromFile(const char * filename) = 0;
	virtual void Draw(int width = 0, int height = 0, UI_IMAGE_RESIZE_POLICY policy = UI_IMAGE_RESIZE_POLICY::NORMAL) = 0;

	virtual UI_TEXTURE_ID GetTextureID() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;

	virtual void Reset() = 0;
	virtual bool IsValid() = 0;
};

// 静态图片显示类
class UIImage: public UIImageBase
{
public:
	UIImage();
	~UIImage();

	bool LoadFromFile(const char* filename) override;
#ifdef UI_CFG_USE_OPENCV
	/* 
	只支持3通道或4通道图像
	如果通道数为3，则一定要转换为RGB格式： cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
	如果通道数为4，则一定要转换为RGBA格式： cv::cvtColor(mat, mat, cv::COLOR_BGRA2RGBA);
	Usage:
		cv::Mat mat = cv::imread("test.png", cv::IMREAD_UNCHANGED);
		int num = mat.channels();
		cv::cvtColor(mat, mat, cv::COLOR_BGRA2RGBA);
		bool ret = LoadFromCV(mat);
	*/
	bool LoadFromCV(const cv::Mat& mat);
	// 前提：图像尺寸没有变化，如图像会动态变化尺寸，则采用LoadFromCV
	bool UpdateTexture(const cv::Mat& mat);
#endif

	void Draw(int width = 0, int height = 0, UI_IMAGE_RESIZE_POLICY policy = UI_IMAGE_RESIZE_POLICY::NORMAL) override;
	int GetWidth() override;
	int GetHeight();
	UI_TEXTURE_ID GetTextureID() override;

	void Reset() override;
	bool IsValid() override;
protected:
	ST_UITexture m_image_texture = { 0 };
};

// GIF图片显示类
class UIGIFImage: public UIImageBase
{
public:
	UIGIFImage();
	~UIGIFImage();

	bool LoadFromFile(const char* filename) override;
	void Draw(int width = 0, int height = 0, UI_IMAGE_RESIZE_POLICY policy = UI_IMAGE_RESIZE_POLICY::NORMAL) override;
	int GetWidth() override;
	int GetHeight();
	UI_TEXTURE_ID GetTextureID() override;

	void Reset() override;
	bool IsValid() override;
protected:
	ST_GIF_TEXTURE m_gif_textures;
	size_t m_cur_index = 0;
	double m_last_index_t = 0.0;
};

using UIImageBasePtr = std::shared_ptr<UIImageBase>;
using UIImagePtr = std::shared_ptr<UIImage>;
using UIGIFImagePtr = std::shared_ptr<UIGIFImage>;

// UIImage工厂类
class UIImageFactory
{
public:
	static UIImageBasePtr LoadFromFile(const char* filename);
};
