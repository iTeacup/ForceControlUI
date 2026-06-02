[APP开发流程.md](https://github.com/user-attachments/files/28505376/APP.md)
# ForceControlUI 新增 IMWindows APP 开发流程

本文档说明如何在 `ForceControlUI.sln` 中新增一个 `IMWindows\APP` 窗口模块。

## 1. 新建 APP 类

文件目录：

```text
msvc/ForceControlUI/
```

新建两个文件：

```text
UIXXX.h
UIXXX.cpp
```

例如：

```text
UIBLEMonitor.h
UIBLEMonitor.cpp
```

APP 类通常继承 `UIBaseWindow`：

```cpp
#pragma once

#include "UIBaseWindow.h"

class UIXXX : public UIBaseWindow
{
public:
    UIXXX(UIMainWindowBase* main_win, const char* title);
    ~UIXXX();

    void Draw() override;
    EUIMenuCategory GetWinMenuCategory() override { return EUIMenuCategory::E_UI_CAT_APP; }
    const char* GetShowShortCut() override { return "Ctrl+5"; }
};
```

窗口界面主要写在 `Draw()` 函数中：

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
    {
        return;
    }

    if (!ImGui::Begin(m_win_title, &m_show, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::Text(u8"这里编写 APP 界面");

    ImGui::End();
}
```

如果需要实时曲线，可以参考已有的 `UICOMMonitor` 或 `UIADCMonitor`。
曲线通常使用 `ImPlot`，历史数据缓存可以使用 `UIPlotDef.h` 中的 `ScrollingBuffer`。

## 2. 在 UIWindowManager.h 中声明 APP

修改文件：

```text
msvc/ForceControlUI/UIWindowManager.h
```

添加类的前置声明：

```cpp
class UIXXX;
```

添加窗口成员指针：

```cpp
UIXXX* m_ui_xxx = nullptr;
```

例如：

```cpp
class UICOMMonitor;
class UIBLEMonitor;
```

```cpp
UICOMMonitor* m_ui_com = nullptr;
UIBLEMonitor* m_ui_ble_monitor = nullptr;
```

## 3. 在 UIWindowManager.cpp 中注册窗口

修改文件：

```text
msvc/ForceControlUI/UIWindowManager.cpp
```

### 3.1 添加头文件引用

在文件顶部添加：

```cpp
#include "UIXXX.h"
```

例如：

```cpp
#include "UICOMMonitor.h"
#include "UIBLEMonitor.h"
```

### 3.2 在 InitWindows() 中创建窗口

在 `UIWindowManager::InitWindows()` 中添加：

```cpp
stbsp_sprintf(buf, u8"%s 窗口名称", ICON_FA_SIGNAL);
m_ui_xxx = new UIXXX(m_ui_main, buf);
m_win_list.push_back(m_ui_xxx);
```

例如：

```cpp
stbsp_sprintf(buf, u8"%s 蓝牙监控", ICON_FA_SIGNAL);
m_ui_ble_monitor = new UIBLEMonitor(m_ui_main, buf);
m_win_list.push_back(m_ui_ble_monitor);
```

只要窗口加入 `m_win_list`，`InitMenus()` 就会自动为它生成菜单项。

## 4. 设置默认显示和停靠位置

仍然修改：

```text
msvc/ForceControlUI/UIWindowManager.cpp
```

在 `UIWindowManager::ResetLayout()` 中添加默认显示：

```cpp
m_ui_xxx->Show();
```

再添加默认停靠位置：

```cpp
ImGui::DockBuilderDockWindow(m_ui_xxx->GetWinTitle(), dock_main_id);
```

例如：

```cpp
m_ui_ble_monitor->Show();
ImGui::DockBuilderDockWindow(m_ui_ble_monitor->GetWinTitle(), dock_main_id);
```

如果不做这一步，APP 仍然可以从菜单打开，但启动时不会默认显示。

## 5. 把新文件加入 Visual Studio 工程

修改文件：

```text
msvc/ForceControlUI/ForceControlUI.vcxproj
```

添加头文件：

```xml
<ClInclude Include="UIXXX.h" />
```

添加源文件：

```xml
<ClCompile Include="UIXXX.cpp" />
```

例如：

```xml
<ClInclude Include="UIBLEMonitor.h" />
<ClCompile Include="UIBLEMonitor.cpp" />
```

如果 APP 使用了额外系统库，还需要加入链接库。

例如 C++/WinRT BLE 功能需要：

```xml
windowsapp.lib
```

示例：

```xml
<AdditionalDependencies>Dwmapi.lib;windowsapp.lib;libUI.lib;rtde.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

通常需要检查这些配置：

```text
Debug|Win32
Release|Win32
Debug|x64
Release|x64
```

## 6. 把新文件加入 Visual Studio 筛选器

修改文件：

```text
msvc/ForceControlUI/ForceControlUI.vcxproj.filters
```

添加头文件筛选器：

```xml
<ClInclude Include="UIXXX.h">
  <Filter>UI\IMWindows\APP</Filter>
</ClInclude>
```

添加源文件筛选器：

```xml
<ClCompile Include="UIXXX.cpp">
  <Filter>UI\IMWindows\APP</Filter>
</ClCompile>
```

这一步只影响 Visual Studio 解决方案资源管理器中的分组，不影响实际编译。

## 7. 编译验证

可以在 Visual Studio 中直接编译，也可以使用 MSBuild。

示例命令：

```powershell
& "D:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" `
  msvc\ForceControlUI.sln `
  /t:ForceControlUI `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /m `
  /v:minimal
```

Debug x64 输出路径：

```text
bin64/ForceControlUId.exe
```

Release x64 输出路径：

```text
bin64/ForceControlUI.exe
```

## 8. 常见注意事项

### 编码问题

如果新建文件中包含中文字符串，建议保存为 `UTF-8 with BOM`。

否则 MSVC 可能出现：

```text
warning C4819
error C2001: 常量中有换行符
```

如果项目或工具链要求严格，也可以尽量使用 ASCII 文件名。

### 析构函数中释放资源

如果 APP 内部启动了线程、串口、BLE、网络连接等资源，需要在析构函数中停止并回收：

```cpp
UIXXX::~UIXXX()
{
    m_exit_flag = true;
    if (m_worker_thread.joinable())
    {
        m_worker_thread.join();
    }
}
```

### UI 线程和后台线程共享数据

如果后台线程更新数据，而 `Draw()` 函数读取数据，建议用 `std::mutex` 保护共享数据：

```cpp
{
    std::lock_guard<std::mutex> lock(m_data_lock);
    local_copy = m_data;
}
```

### 菜单分类

普通 APP 窗口一般返回：

```cpp
EUIMenuCategory::E_UI_CAT_APP
```

工具类窗口可以使用：

```cpp
EUIMenuCategory::E_UI_CAT_TOOL
```

## 总结

新增 APP 的核心流程：

```text
1. 新建 UIXXX.h 和 UIXXX.cpp。
2. 继承 UIBaseWindow，并实现 Draw()。
3. 在 UIWindowManager.h 中声明类和成员指针。
4. 在 UIWindowManager.cpp 中 include、创建窗口、加入 m_win_list。
5. 如需默认显示，在 ResetLayout() 中 Show 并 Dock。
6. 在 ForceControlUI.vcxproj 中加入 .h 和 .cpp。
7. 在 ForceControlUI.vcxproj.filters 中加入 UI\IMWindows\APP 分组。
8. 编译并验证。
```
