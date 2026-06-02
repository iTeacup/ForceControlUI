#pragma once
#include <vector>
#include "Config.h"

#define UI_DATA_DIR		"data" // 数据根目录，必须为单层目录

//////////////////////////////////////////////////////////////////////////
// 此头文件定义的结构体不需要通过CPS发送，因此可以使用STL对象
//////////////////////////////////////////////////////////////////////////
typedef enum
{
	E_UI_POLL_MODE, // 性能模式
	E_UI_WAIT_MODE, // 平衡模式
	E_UI_WAIT_TIMEOUT_MODE // 固定模式
}E_UI_MODE;

#ifdef UI_RENDER_ENGINE_OPENGL
#define UI_TEXTURE_ID	unsigned int
#else
#define UI_TEXTURE_ID	void*
#endif

typedef struct
{
	UI_TEXTURE_ID	image_texture_id;
	int				image_width;
	int				image_height;
	int				channels;
	unsigned char*	image_data;
}ST_UITexture;

typedef struct
{
	std::vector<ST_UITexture>	vec_texture;
	std::vector<int>			vec_delay_ms;
}ST_GIF_TEXTURE;
