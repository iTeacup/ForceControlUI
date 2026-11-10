# NOKOV Windows C++ SDK

来源版本：`XING_Windows_C++_SDK_4.1.0.5634`，取自本项目已验证的 SDK 包。

仅保留当前 NokovServer 构建和运行需要的原始文件：

- `include/NokovSDKClient.h`
- `include/NokovSDKTypes.h`
- `lib/nokov_sdk.lib`
- `lib/nokov_sdk.dll`

NokovServer 默认从此目录引用 SDK，使用仓库相对路径，无需放到某个用户的桌面。编译后自动复制 SDK DLL 到 `bin64`。

需要测试其他完整 SDK 时，仍可通过 MSBuild 参数 `/p:NokovSdkDir=完整SDK目录` 覆盖默认路径。替换时需成套更新头文件、链接库及 DLL。

此处 SDK 接口和二进制文件保留原始内容；版权属于原供应商。相机驱动、XINGYING 软件、设备工程和标定文件由使用设备的电脑另行准备。
