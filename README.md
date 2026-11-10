# ForceControlUI

Windows x64 上位机，包含 NOKOV 动捕、OptiTrack、BLE 四通道电压及其他设备窗口。

安装 Visual Studio 2022 的“使用 C++ 的桌面开发”，打开 `ForceControlUI/msvc/ForceControlUI.sln`，选择 **Debug / x64** 并生成。编译后自动准备 `ForceControlUI/bin64`，已有运行配置不会被覆盖。

- [下载、编译与使用教程](UI下载编译与使用教程.md)
- [APP 开发与删除流程](APP开发与删除流程.md)
- [NOKOV 模块说明](ForceControlUI/msvc/NokovServer/README.md)
- [x64 运行依赖说明](ForceControlUI/runtime/README.md)

项目统一按文件职责组织：`share` 保存共用代码及 SDK 头文件，`include` 保存项目公共定义，`deps` 保留第三方库原始代码，`lib64` 保存 x64 链接库，`runtime` 保存 x64 运行 DLL，`msvc` 保存工程代码及默认配置，`bin64` 保存本地生成文件和运行数据。
