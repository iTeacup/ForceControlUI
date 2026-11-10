# x64 运行依赖

本目录保存 Debug 上位机和 OPT 服务需要的业务 DLL，纳入 Git。编译后由 `msvc/RuntimeFiles.targets` 配合各工程的依赖清单复制到 `bin64`。

| 文件 | 用途 |
| --- | --- |
| cpsapi.dll、libbus.dll | x64 上位机及各服务的通信库，与 lib64/cpsapi.lib 配套 |
| rtded.dll | 当前 Debug 上位机的 UR RTDE 运行库 |
| boost_thread-vc143-mt-gd-x64-1_88.dll | rtded.dll 的配套 Boost 线程库 |
| modbus-5.dll | 当前上位机的 Modbus 运行库 |
| NatNetLib.dll | OptServer 的 NatNet 运行库 |

这些文件取自当前已验证的 x64 运行环境，与 `lib64` 中的链接库配套。替换版本时同时核对对应链接库和传递依赖。

这里的 x64 通信 DLL 来自项目初始提交中已验证的 bin64 版本。`bus_server` 自身为 32 位程序，其目录中的同名 DLL 是 32 位，必须留给总线单独使用，不能复制给 x64 上位机。NOKOV SDK 的运行库位于 `deps/nokov/lib`。

Visual C++ 系统调试运行库由使用者安装的 Visual Studio 2022 C++ 开发环境提供，不在本目录复制。`bin64` 中的程序输出、个人配置和采集数据继续忽略。
