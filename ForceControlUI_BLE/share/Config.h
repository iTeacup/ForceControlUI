#pragma once
// 定义渲染引擎, 默认OpenGL
#ifndef UI_RENDER_ENGINE_D3D11
#define UI_RENDER_ENGINE_OPENGL
#endif

#ifdef UI_RENDER_ENGINE_OPENGL
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "opengl32.lib")
#else
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#endif

// 定义此宏以启用CPS连接
#define UI_CFG_ENABLE_CPS


// 定义此宏以启用帮助文档
//#define UI_CFG_BUILD_DOC

// 定义此宏以启用ImPLOT
#define UI_CFG_USE_IMPLOT

// 定义此宏以启用Notification
#define UI_CFG_USE_NOTIFICATION

// 定义此宏以启用OpenCV图片支持
//#define UI_CFG_USE_OPENCV

// 定义此宏以将log打印到控制台，默认打印到UI
//#define UI_CFG_LOG_TO_CONSOLE

// 定义此宏，以启用3D绘制
//#define  UI_CFG_ENABLE_3D
