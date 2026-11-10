# ForceControlUI 下载、编译与使用教程

适用于首次获取项目并使用软件。APP 的开发、删除和临时禁用，见 [APP开发与删除流程](APP开发与删除流程.md)。

## 1. 下载项目

仓库地址：[iTeacup / ForceControlUI](https://github.com/iTeacup/ForceControlUI)。

1. 打开仓库，选择 `main` 分支。
2. 点击 **Code → Download ZIP**，下载后全部解压。
3. 在解压目录中找到 `ForceControlUI/msvc/ForceControlUI.sln`。

也可以使用 Git 下载：

```powershell
git clone https://github.com/iTeacup/ForceControlUI.git
```

下载后的仓库根目录内还有一层 ForceControlUI 工程目录，注意区别。

## 2. 安装编译环境

安装 **Visual Studio 2022**，勾选“使用 C++ 的桌面开发”，包含：

- MSVC v143 x64/x86 工具。
- Windows 10 或 Windows 11 SDK。

当前使用 Debug 版本，需要上述开发环境提供调试运行库，普通 VC++ 运行库安装包不能替代。运行软件时无需一直打开 VS。

## 3. 编译项目

1. 打开 `ForceControlUI/msvc/ForceControlUI.sln`。
2. 选择 **Debug / x64**。
3. 点击 **生成 → 生成解决方案**，在输出窗口确认生成成功。

程序输出到 `ForceControlUI/bin64`，上位机为 **ForceControlUId.exe**。所需 DLL 和默认 JSON 配置会自动复制到该目录，无需手工准备；已有运行配置不会被覆盖。

也可在 VS 中将 ForceControlUI 设为启动项目，按 F5 调试。

## 4. 启动 APP

先完成设备连接、通电及必要配置。地址、串口等参数按实际设备填写；服务的运行配置在 bin64 中对应的 JSON 文件里。

### 4.1 简单 APP：直接连接设备

1. 打开 `ForceControlUI/bin64/ForceControlUId.exe`。
2. 在“应用”菜单中打开需要的 APP，连接设备并使用。

这类 APP 直接在上位机内连接设备，不需要单独启动总线服务程序。

### 4.2 使用总线服务的 APP

1. 打开 `ForceControlUI/bus_server/bus_server.exe`，保持运行。
2. 打开 `ForceControlUI/bin64` 中该 APP **对应的服务 EXE**，保持运行。
3. 打开 `ForceControlUId.exe`，进入对应 APP，连接设备并使用。

启动顺序：**bus_server → 对应服务 EXE → 上位机 APP**。只启动需要的服务，同一电脑只需一个 bus_server 实例。若设备需要厂商软件提供数据，先按设备要求开启其数据输出。

## 5. 保存与退出

在对应 APP 中选择保存或采集功能。保存位置以该 APP 的设置和提示为准，注意保留个人配置和历史数据。

退出前先停止采集，等待保存完成，再断开设备、关闭上位机及不再使用的服务。

## 6. 更新项目

Git 下载的项目，在仓库根目录妥善保存本地修改、关闭相关程序后执行：

```powershell
git status
git pull --ff-only origin main
```

然后重新生成解决方案。已有 bin64 配置会保留，需要时对照工程目录中的新模板调整。

ZIP 下载的项目需重新下载、解压和编译，再迁移自己的配置与数据；不能直接使用 git pull。

## 7. 常见问题

| 问题 | 处理方式 |
| --- | --- |
| 缺少 v143 或 Windows SDK | 在 VS Installer 中补装 C++ 桌面开发组件 |
| 缺少 MSVCP140D.dll、ucrtbased.dll 等 | 安装配套 VS C++ 开发环境 |
| 缺少业务 DLL 或默认配置 | 重新生成对应项目，检查输出窗口的复制错误 |
| 生成时无法覆盖 EXE/DLL | 停止采集并关闭占用文件的程序，再生成 |
| bus_server 启动后退出 | 检查是否重复启动、4000 端口是否被占用；在其目录用终端启动查看输出 |
| APP 显示设备离线或无数据 | 检查对应服务是否运行、设备连接及配置是否正确 |
| 修改配置模板后参数没变 | 生成不会覆盖已有运行配置，修改 bin64 中的对应 JSON |
