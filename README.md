# virtual-screen

本机虚拟屏控制与预览工具（**Qt Widgets / C++**）。

中间预览区尽量铺满；标题栏约 28px（含最小化 / 最大化·还原 / 关闭）；无底部状态栏。

## 运行

### 安装包（推荐发版）

```powershell
.\pack_setup.ps1
```

输出：`dist\VirtualScreen-Setup-v1.1.exe`。安装时会：

1. 写入程序目录（默认 `Program Files\VirtualScreen`）与开始菜单 / 桌面快捷方式  
2. **以管理员静默安装**捆绑的 Parsec VDD 驱动（`vendor\parsec-vdd\…`）

卸载只删本程序，**不会**卸掉系统里的 Parsec VDD。

### 开发/本机目录版（调试）

```text
dist\VirtualScreen.exe
```

同目录需有 Qt DLL（`build_qt.ps1` 会用 windeployqt 部署）。

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

示例见 `dist\profiles\`（单屏 / 双屏 / 三屏）。不用时点顶栏**清除**卸掉虚拟屏。

## 说明

- **虚拟屏驱动**：仅使用 [Parsec VDD](https://github.com/nomi-san/parsec-vdd)（`Parsec Virtual Display Adapter`）
- **仓库已捆绑驱动安装包**：`vendor\parsec-vdd\parsec-vdd-0.45.0.0.exe`（官方原样转发，约 500KB）
  - 一键安装：`.\scripts\install_parsec_vdd.ps1`（会要管理员）
  - 或 GUI：未装驱动时点「安装捆绑驱动」
  - `build_qt.ps1` 会把安装包拷到 `dist\parsec-vdd\`，整份 dist 带走即可
- 使用本程序「应用」时建议先关掉官方 ParsecVDisplay，避免抢控
- **保持 VirtualScreen 运行**：Parsec 需要约 100ms 保活 ping；关掉本程序后虚拟屏约 1 秒会自动卸掉
- 普通启动**不强制**管理员；点「应用 / 清除 / 安装驱动」时再弹 UAC
- **缩放%**：应用配置时会按屏写入系统 DPI；失败时可到 Windows「显示设置」手动调
- 显示设置里若出现 `1|2`，说明两块屏在「复制」模式，请改成「扩展这些显示器」

## 许可证

- **本仓库代码**： [MIT](LICENSE)
- **捆绑的 Parsec VDD 安装包**：版权归 Parsec；本仓库仅原样转发，详见 `vendor/parsec-vdd/`
