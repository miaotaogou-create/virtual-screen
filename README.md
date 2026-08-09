# virtual-screen

本机虚拟屏控制与预览工具（**Qt Widgets / C++**）。

中间预览区尽量铺满；标题栏约 28px（含最小化 / 最大化·还原 / 关闭）；无底部状态栏。

## 运行

### 开发/本机目录版（推荐调试）

```text
dist\VirtualScreen.exe
```

同目录需有 Qt DLL（`build_qt.ps1` 会用 windeployqt 部署）。**整份 `dist\` 文件夹**拷到别的电脑即可跑（需目标机有 VC++ 运行库，或把 `msvcp140.dll` / `vcruntime140*.dll` 一并放进 `dist`）。

### 单文件绿色版（Enigma Virtual Box）

Qt 不是静态单文件；做法是 **windeployqt 收齐 DLL → EVB 虚拟进主程序**。

```powershell
.\pack_evb.ps1
```

脚本会编译、准备 `build\evb_stage`，并打开 EVB。按控制台里那 **5 步**点完即可（比自动点对话框靠谱）。  
输出：`dist\portable\VirtualScreen.exe`。脚本会把 `profiles\` 与 `config.json` 预先放到 exe **旁边**（勿打进虚拟盒）。

首次在 EVB 里 Save Project 为 `pack\VirtualScreen.evb` 后，以后也可用：

```text
enigmavbconsole.exe pack\VirtualScreen.evb -input build\evb_stage\VirtualScreen.exe -output dist\portable\VirtualScreen.exe
```

## 编译

依赖：Qt 5.14（msvc2017_64）+ VS2019。

```powershell
.\build_qt.ps1
```

## 操作（傻瓜路径）

1. 上方 **方案** 下拉选预设（或用当前配置）
2. 点顶栏 **应用**（首次会说明并弹 UAC）
3. 应用成功后会自动打开预览；也可随时点 **预览:开/关**

中间空白区有步骤卡；未装驱动时可点「打开驱动下载页」。

- 顶栏：**应用 / 清除 / 设置**，以及窗口按钮
- 双击标题栏：最大化 / 还原；**拖窗口边缘**可调整大小
- **虚拟屏 Tab**：切换预览；画面等比铺满（类似 VMware Fit）

### 按项目改规格

在**设置**里可以：

- **添加虚拟屏 / 删除最后一块**（1～8 块）
- **配置方案下拉 / 另存为… / 浏览…**
- **保存方案**：写入启动默认的 `config.json`（真正创建屏仍点顶栏「应用」）

示例见 `dist\profiles\`（单屏 / 双屏 / 三屏）。若电脑变卡，点顶栏**清除**禁用驱动（需管理员）。

## 说明

- 依赖已安装的 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)
- 未装驱动时：可用管理员运行 `scripts\install_vdd.ps1`，或从官网 Releases 安装
- 普通启动**不强制**管理员；点「应用 / 清除」时再弹 UAC
- **缩放%**：应用配置时会按屏写入系统 DPI（DisplayConfig，档位 100/125/150…）；个别驱动/系统可能失败，可到 Windows「显示设置」核对
