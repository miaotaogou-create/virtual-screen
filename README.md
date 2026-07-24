# virtual-screen

本机「虚拟显示器」控制与预览工具：在没有对应物理屏时，用系统级虚拟屏做任意分辨率 / 缩放的 UI、双屏布局测试，并在本机窗口里缩小预览画面。

与具体业务仓库解耦；分辨率、缩放全部由配置决定，不写死某一产品的规格。

## 它解决什么

| 能力 | 说明 |
|------|------|
| 系统虚拟屏 | 依赖已安装的 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)（IddCx），让 Windows 真正多出可设分辨率的显示器 |
| 任意分辨率 / 缩放 | 配置文件或 GUI 填写宽×高、缩放%、刷新率；默认示例为 2 块屏 |
| 预览 | 把虚拟屏（若无则非主屏）画面抓下来，按比例缩小显示在本工具窗口里——否则虚拟屏对你来说是「空气」 |
| 应用 / 清除 | GUI 一键写驱动配置并重启设备，或禁用虚拟显示设备 |

被测程序（例如任意 Qt/桌面应用）应把窗口拖到虚拟屏上运行；本工具只负责造屏 + 让你看见。

## 环境

- Windows 10/11
- Python 3.10+（仅用标准库 + Win32 API，无第三方包）
- **应用/清除需要管理员权限**（改 PnP 设备、写 `C:\VirtualDisplayDriver\`）
- 预览、枚举监视器：普通权限即可

## 安装虚拟显示驱动（一次性）

本仓库**不自带**驱动，也不重新实现 IddCx。

1. 打开上游发布页：https://github.com/VirtualDrivers/Virtual-Display-Driver/releases  
2. 按上游说明安装**已签名**驱动  
3. 确认出现目录 `C:\VirtualDisplayDriver\`，或在设备管理器「显示适配器」中能看到虚拟显示相关设备  
4. 用 `run_admin.bat` 启动本工具，再点「应用」

未装驱动时：仍可打开 GUI，预览本机已有副屏；点「应用」会提示先安装驱动。

## 怎么用

```text
# 普通权限：预览 / 改配置
run.bat

# 管理员：应用 / 清除虚拟屏
run_admin.bat
```

或：

```text
python -m vscreen
python selfcheck.py
```

### 配置

首次运行会从 `config.example.json` 生成 `config.json`。也可直接改 JSON：

```json
{
  "displays": [
    { "width": 1920, "height": 1080, "scale": 125, "hz": 60, "label": "虚拟屏1" },
    { "width": 1920, "height": 1080, "scale": 100, "hz": 60, "label": "虚拟屏2" }
  ],
  "preview_max_height": 320,
  "preview_fps": 5,
  "vdd_settings_path": "C:\\VirtualDisplayDriver\\vdd_settings.xml"
}
```

GUI 里改完可「保存配置」；「应用」会先保存再写 `vdd_settings.xml`、重启驱动设备、设置分辨率/位置，并尝试设置每屏 DPI 缩放。

推荐流程：

1. 管理员启动 → 填两块屏的宽高与缩放 → **应用**  
2. 在 Windows「设置 → 系统 → 显示」确认多出两块屏（必要时微调排列）  
3. 把被测程序窗口拖到虚拟屏（或设为在那块屏全屏）  
4. 看本工具里的**预览**核对布局；本机物理屏较小只影响预览缩放，不影响虚拟屏逻辑分辨率  

**清除**：禁用虚拟显示 PnP 设备（可在设备管理器再启用）。

## 和真机的差异 / 已知限制

- **不是**完整显示器硬件仿真：无真实面板、色准、触控、EDID 细节以驱动为准。  
- **缩放（DPI）**依赖系统 CCD API；个别机器/驱动组合可能失败，需在系统显示设置里手动设缩放。  
- **预览**是定时抓屏缩小，有延迟，且不是独立合成器；光标/HDR/受保护内容可能不准或抓不到。  
- 虚拟屏画面默认只存在于「那块逻辑屏」；没有预览时你会感觉在操作空气——所以本工具把预览算进 MVP。  
- 「疑似虚拟」靠适配器/监视器名称关键字匹配；名称不符时预览会退化为「所有非主屏」。  
- 应用/清除需管理员；写 `C:\VirtualDisplayDriver\` 失败时检查权限与驱动是否已装。  
- 本机已有物理副屏时，预览可能同时出现物理副屏与虚拟屏，以列表区标注为准。

## 目录

```text
vscreen/          # 程序（配置、VDD、显示 API、GUI）
config.example.json
selfcheck.py      # 配置 + 抓屏最小自检
run.bat / run_admin.bat
README.md
```

## 设计取舍（YAGNI）

- 不自研显示驱动；复用现成 IddCx 驱动。  
- 不做触控板/完整输入仿真。  
- 不绑定任何业务仓库的分辨率。
