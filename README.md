# virtual-screen

本机「虚拟显示器」控制与预览工具：在没有对应物理屏时，用系统级虚拟屏做任意分辨率 / 缩放的 UI、双屏布局测试，并在本机窗口里缩小预览画面。

与具体业务仓库解耦；分辨率、缩放全部由配置决定，不写死某一产品的规格。

## 绿色便携版（推荐）

单文件 exe（已把 Python / tkinter 等打进去），解压/拷贝即可用：

```text
dist\VirtualScreen.exe
```

同目录可放 `config.json`（没有则首次自动生成）。**应用 / 清除**请右键「以管理员身份运行」，或用：

```text
run_exe_admin.bat
```

仅预览可用：

```text
run_exe.bat
```

重新打包：

```text
scripts\build_exe.bat
```

## 它解决什么

| 能力 | 说明 |
|------|------|
| 系统虚拟屏 | 依赖 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)（IddCx），让 Windows 真正多出可设分辨率的显示器 |
| 任意分辨率 / 缩放 | 配置文件或 GUI 填写宽×高、缩放%、刷新率；默认示例为 2 块屏 |
| 预览 | 把虚拟屏（若无则非主屏）画面抓下来，按比例缩小显示在本工具窗口里 |
| 应用 / 清除 | GUI 一键写驱动配置并重启设备，或禁用虚拟显示设备 |

被测程序应把窗口拖到虚拟屏上运行；本工具只负责造屏 + 让你看见。

## 环境

- Windows 10/11
- 便携 exe：**不需要**本机预装 Python
- 源码运行：Python 3.10+（标准库 + Win32 API）
- **应用 / 清除 / 装驱动需要管理员**（会弹 UAC）
- 预览：普通权限即可

## 安装虚拟显示驱动（一次性）

本仓库不内嵌驱动二进制，提供一键安装脚本（下载上游签名驱动 + nefcon 静默安装，会弹 UAC）：

```text
scripts\install_vdd.bat
```

装好后应能在设备管理器看到 **Virtual Display Driver**，并存在 `C:\VirtualDisplayDriver\`。

也可手动从上游安装：https://github.com/VirtualDrivers/Virtual-Display-Driver/releases

## 怎么用（源码）

```text
run.bat
run_admin.bat
python -m vscreen
python selfcheck.py
```

### 配置

首次运行会从 `config.example.json` 生成 `config.json`。示例：

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

推荐流程：

1. 管理员启动 → 填两块屏的宽高与缩放 → **应用**  
2. 在 Windows「设置 → 系统 → 显示」确认多出虚拟屏  
3. 把被测程序窗口拖到虚拟屏  
4. 在本工具**预览**里核对布局  

**清除**：禁用虚拟显示 PnP 设备。

## 和真机的差异 / 已知限制

- 不是完整显示器硬件仿真（色准、触控、EDID 细节以驱动为准）。  
- DPI 缩放依赖系统 API，失败时可在系统显示设置里手动设。  
- 预览是定时抓屏缩小，有延迟；HDR/受保护内容可能抓不到。  
- 「疑似虚拟」靠名称关键字；不匹配时预览退化为所有非主屏。  
- 应用/清除需管理员。

## 目录

```text
dist/VirtualScreen.exe   # 绿色便携 GUI
vscreen/                 # 源码
scripts/install_vdd.*    # 驱动一键安装（UAC）
scripts/build_exe.bat    # 重新打包 exe
config.example.json
selfcheck.py
```

## 设计取舍

- 不自研显示驱动；复用现成 IddCx 驱动。  
- 不做触控板/完整输入仿真。  
- 不绑定任何业务仓库的分辨率。
