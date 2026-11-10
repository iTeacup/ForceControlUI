# ForceControlUI APP 开发、删除与临时禁用

本文分为三部分：新增开发、完整删除、暂时禁用与恢复。下载、编译和使用软件见 [UI下载编译与使用教程](UI下载编译与使用教程.md)。

## 第一部分：开发新增 APP

### 1.1 创建窗口类

在 `ForceControlUI/msvc/ForceControlUI/` 新建 `UIXXX.h` 和 `UIXXX.cpp`。以下窗口管理器、主工程文件也位于这个目录。

`UIXXX.h`：

```cpp
#pragma once
#include "UIBaseWindow.h"

class UIXXX : public UIBaseWindow
{
public:
    UIXXX(UIMainWindowBase* main_win, const char* title);
    ~UIXXX() override;
    void Draw() override;
    EUIMenuCategory GetWinMenuCategory() override
    {
        return EUIMenuCategory::E_UI_CAT_APP;
    }
};
```

`UIXXX.cpp`：

```cpp
#include "UIXXX.h"
#include "imgui/imgui.h"

UIXXX::UIXXX(UIMainWindowBase* main_win, const char* title)
    : UIBaseWindow(main_win, title)
{
}

UIXXX::~UIXXX()
{
}

void UIXXX::Draw()
{
    if (!m_show)
        return;
    if (!ImGui::Begin(m_win_title, &m_show))
    {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(u8"这里编写 APP 界面");
    ImGui::End();
}
```

### 1.2 注册窗口和默认显示

在 `UIWindowManager.h` 的类外增加前置声明，在类内增加成员：

```cpp
class UIXXX;                      // UIWindowManager 类外
UIXXX* m_ui_xxx = nullptr;         // UIWindowManager 类内
```

在 `UIWindowManager.cpp` 顶部添加：

```cpp
#include "UIXXX.h"
```

在 `InitWindows()` 中添加：

```cpp
stbsp_sprintf(buf, u8"%s 窗口名称", ICON_FA_SIGNAL);
m_ui_xxx = new UIXXX(m_ui_main, buf);
m_win_list.push_back(m_ui_xxx);
```

在 `ResetLayout()` 中添加：

```cpp
m_ui_xxx->Show();
ImGui::DockBuilderDockWindow(m_ui_xxx->GetWinTitle(), dock_main_id);
```

加入 m_win_list 后，框架自动生成“应用”菜单项；Show 和 Dock 让窗口默认显示在主区域并形成顶部标签，无需另外注册顶部菜单。

### 1.3 加入工程和筛选器

在 `ForceControlUI.vcxproj` 对应的头文件、源文件 ItemGroup 中分别加入：

```xml
<ClInclude Include="UIXXX.h" />
<ClCompile Include="UIXXX.cpp" />
```

在 `ForceControlUI.vcxproj.filters` 中加入：

```xml
<ClInclude Include="UIXXX.h">
  <Filter>UI\IMWindows\APP</Filter>
</ClInclude>
<ClCompile Include="UIXXX.cpp">
  <Filter>UI\IMWindows\APP</Filter>
</ClCompile>
```

也可在 VS 的 UI/IMWindows/APP 筛选器中“添加 → 现有项”，再确认以上条目齐全。只改 .filters 不会让源文件参与编译。

### 1.4 编译和检查

1. 关闭正在运行的上位机，选择 **Debug / x64 → 生成解决方案**。
2. 运行 `ForceControlUI/bin64/ForceControlUId.exe`，检查应用菜单、默认窗口和顶部标签。
3. 检查功能及退出是否正常；布局未更新时使用“窗口 → 重置窗口布局”。

实现时注意：

- 析构函数停止采集、线程和连接；一个对象只加入一次 m_win_list，由框架统一释放，不重复 delete。
- 后台回调与 UI 共享数据时加锁；新快捷键避免与已有 APP 冲突。
- 新增含中文的 C++ 文件可用 UTF-8 BOM，既有文件保留原编码；图标按上例通过 `%s` 插入。
- 新依赖追加到现有配置，不覆盖原有链接库。BLE 使用的 windowsapp.lib 已在当前主工程中。

## 第二部分：完整删除 APP

### 2.1 简单 APP：以蓝牙监控为例

先停止采集、等待保存结束并关闭上位机，然后逐项清理：

| 位置 | 删除内容 |
| --- | --- |
| UIWindowManager.cpp 顶部 | `#include "UIBLEMonitor.h"` |
| InitWindows() | 蓝牙监控的标题格式化、new UIBLEMonitor、push_back 三句 |
| ResetLayout() | `m_ui_ble_monitor->Show()` 和对应 DockBuilderDockWindow |
| UIWindowManager.h | `class UIBLEMonitor;`、成员 `m_ui_ble_monitor`，以及专用 getter（如有） |
| ForceControlUI.vcxproj | UIBLEMonitor.h 的 ClInclude、UIBLEMonitor.cpp 的 ClCompile 条目 |
| ForceControlUI.vcxproj.filters | 上述两个文件的完整筛选器条目，包含 Filter 子节点 |
| 源文件 | `ForceControlUI/msvc/ForceControlUI/UIBLEMonitor.h` 和 `.cpp` |

接着全仓库搜索 `UIBLEMonitor`、`m_ui_ble_monitor`，清理其他模块的调用，以及该 APP 独有的辅助代码、配置读写字段、资源和旧的专用构建文件。共用的框架文件、库、公共配置及历史采集数据保留。

重新生成并启动，确认：

- 应用菜单和顶部标签不再出现蓝牙监控。
- “显示所有窗口”或“重置窗口布局”不会使它重新出现，其他 APP 正常。

**到这里，BLE 这类没有独立配套服务的简单 APP 就删除完成了，不需要执行下面的 NOKOV 附加步骤。**

菜单由 m_win_list 自动生成，不需要另删一份菜单注册，也不要在 Destroy 中额外添加 delete。VS“从项目中移除”后，还需确认磁盘上的文件已删除。

### 2.2 带独立服务的 APP：NOKOV 额外清理

先按 2.1 清理窗口层，将示例中的类和成员替换为 `UINokov`、`m_ui_nokov`；再关闭 NokovServer，追加以下步骤，最后统一编译验收。

**① 移除订阅和设备声明。**

在 `ForceControlUI/msvc/ForceControlUI/CPSHandler.cpp` 移除：

```cpp
#include <CPSNokovDef.h>
// OnConnected() 中的订阅：
m_cpsapi->SubscribeDevice(nokov::Device);
```

在 `ForceControlUI/bus_server/devices.json` 删除 id 为 **780** 的 NokovServer 对象，检查 JSON 逗号。保留其他设备、DEV_MIN、DEV_MAX 和总线程序。

**② 移除服务工程。**

在解决方案中右键 NokovServer → 移除，再删除 `ForceControlUI/msvc/NokovServer/`。确认 .sln 中该项目的 Project 块、四条 Debug/Release x64 配置映射，以及其他项目对它的依赖均已移除。

对应项目 GUID：`{AB625148-84A3-4DE4-8528-D2E251B79972}`。仅取消“生成”勾选不属于完整删除。

**③ 移除专用协议和 SDK。**

确认没有其他模块使用后，删除以下文件及其工程/筛选器引用：

- `ForceControlUI/include/CPSNokovDef.h`。
- `ForceControlUI/share/Nokov/`。
- `ForceControlUI/lib64/nokov_sdk.lib`。
- `ForceControlUI/runtime/nokov_sdk.dll`。

同时清理残留的 NOKOV 专用链接库、RuntimeDependency 和配置条目。服务默认 nokovcfg.json 随服务工程目录移除。共用的 RuntimeFiles.targets、CPSAPI、cpsapi.lib、通信 DLL 及其他 SDK 保留。

**④ 清理旧输出和失效说明。**

在 bin64 中清理旧的 `NokovServer.exe`、`NokovServer.pdb`、`nokov_sdk.dll` 和 `nokovcfg.json`，避免误启动；清理该服务的中间输出和失效文档链接。历史采集数据按需要保留。

**⑤ 验收。**

搜索 `UINokov`、`m_ui_nokov`、`NokovServer`、`CPSNokovDef`、`nokov_sdk`、`nokovcfg`，确认无失效代码、工程和部署引用。生成整个 Debug x64 解决方案，重启总线和上位机，确认其他 APP 正常，NOKOV 菜单与顶部标签均消失。

**窗口层和这五项都完成后，带独立服务的 NOKOV 才算删除完整。**

## 第三部分：暂时禁用与恢复 APP

暂时不用时保留源码及工程条目，通过注释窗口创建和默认显示，让它在应用菜单和顶部窗口中都不出现。

### 3.1 禁用

以蓝牙监控为例，修改 `ForceControlUI/msvc/ForceControlUI/UIWindowManager.cpp`。

在 `InitWindows()` 中将三句一起注释：

```cpp
//stbsp_sprintf(buf, u8"%s 蓝牙监控", ICON_FA_SIGNAL);
//m_ui_ble_monitor = new UIBLEMonitor(m_ui_main, buf);
//m_win_list.push_back(m_ui_ble_monitor);
```

在 `ResetLayout()` 中同时注释：

```cpp
//m_ui_ble_monitor->Show();
//ImGui::DockBuilderDockWindow(m_ui_ble_monitor->GetWinTitle(), dock_main_id);
```

保留 include、前置声明、初始化为 nullptr 的成员、.h/.cpp 和工程条目。如果其他模块直接调用该窗口，也应同步处理这些调用，避免空指针访问。

**new、push_back、Show、Dock 必须成组处理**，不能只注释其中一处。修改后重新生成并启动，确认应用菜单和顶部标签都不再出现；必要时重置窗口布局。

NOKOV 同样处理，把示例名称替换为 `UINokov`、`m_ui_nokov` 及对应标题；再正常关闭 NokovServer，暂时不启动它。源码、SDK、服务工程、设备声明和配置均保留。

### 3.2 恢复

1. 取消 InitWindows 三句和 ResetLayout 两句的注释，确保只创建、加入清单一次。
2. 恢复曾同步处理的其他调用。
3. 重新生成；带服务的 APP 启动对应服务，再启动上位机。
4. 确认应用菜单、默认窗口、顶部标签及设备功能恢复正常。
