# NOKOV 动捕 APP

## 工程组织

解决方案：msvc/ForceControlUI.sln。
服务：msvc/NokovServer/NokovServer.vcxproj，按 Header Files、Source Files、Resource Files 分组。
窗口：msvc/ForceControlUI/UINokov.h/.cpp，位于 UI/IMWindows/APP，默认停靠主区域。
CSV 序列化为窗口 CPP 内部函数。

运行文件为 bin64/NokovServer.exe、nokovcfg.json 和原来的 ForceControlUId.exe。
设备 ID 为 780。默认使用仓库 deps/nokov 中的 SDK，通过公共 RuntimeFiles.targets 自动复制运行 DLL 和默认配置；已有运行配置不覆盖。仍可用 /p:NokovSdkDir=SDK目录 指定其他完整 SDK。

## 使用与行为

单机启动顺序：总线 → XINGYING（开启 SDK）→ NokovServer.exe → ForceControlUId.exe。
总线默认 127.0.0.1:4000，SDK 默认连接广播网卡 10.1.1.198；变更网络时修改 nokovcfg.json。
服务启动后等待“连接NOKOV”，不自动连接 SDK。控制台 q 回车、关闭按钮及 Ctrl+C 都执行退出清理。
当前协议版本为 6，升级时同时更新服务和上位机。

SDK 回调复制完整帧，进入有界队列，后台逐帧发送；上位机逐帧接收和后台保存，坐标预览 10 Hz 更新。
支持选点/全部点、单次保存和连续采集，隐藏窗口不停止采集。停止后排空已接受数据，断开前先完成写入。
勾选“记录全部点”会选中全部点；取消任一点时自动退出全选，再次选齐全部点时恢复全选。
取消“记录全部点”会清空选择；缺失的已选点仍可取消选择。采集过程中锁定选择。
溢出、断流、序号异常、身份变化、时间回退及写入失败会提示并结束本轮；底部日志显示操作和保存结果。

## 数据文件

正常完成仅生成 data.csv 和 marker_map.json；异常结束时尝试额外生成 error.json。历史记录不修改。
CSV 九列：frame,time_s,row_type,asset_name,marker_name,present,x,y,z。
每帧一条 frame 行，随后为点记录；保留空帧，所选缺失点 present=0、XYZ 留空，坐标单位 mm。
SDK 教程《The tutorials of XingYingSDK .pdf》第 2 页第 3(11) 项定义 iTimeStamp 为自 1970-01-01 起的绝对毫秒时间戳。
将 SDK 原始时间戳减去本轮首个保存帧的时间戳，再除以 1000 得到 time_s，固定三位小数；每轮从 0.000 开始。
frame 直接取 SDK iFrame，不重新编号；time_s 不由帧号、设定帧率或上位机接收 Hz 推算。
marker_map.json 保存原始起始时间戳、单位、会话和 ID/名称对应关系。时间戳具体对应曝光、重建或发送的哪个物理时刻尚未确认。
测量时间只保存每帧 SDK 原始时间戳与一个首帧基准；回退检查在入队前完成，不额外维护最后写入时间戳。
断流检测、接收 Hz、10 Hz 预览、文件刷新和服务重试使用 steady_clock，计时不写入测量文件或 error.json。
本地生成的会话和目录编号只用于区分连接及文件，不作为采集时间。fLatency、Timecode 和 TimecodeSubframe 暂不提取。
CSV 为 UTF-8 BOM，名称按标准规则转义。原始大整数时间戳读取时注意精度。

## 名称识别

连接时读取资产描述。每帧仅在 MarkerSet 与 LabeledMarkers 坐标完全一致且唯一时关联名称；不跨帧按距离猜测。
重合、零坐标占位或描述不一致时不强行命名。名称未确认时不能采集，采集中身份变化会结束本轮。
修改或重新创建资产后重新连接。原始数字 ID 仅用于内部关联及对应表，不出现在 UI 和 CSV 中。
