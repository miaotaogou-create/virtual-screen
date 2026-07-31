# virtual-screen

本机虚拟屏控制与预览工具（**Qt Widgets / C++**）。

中间预览区尽量铺满；标题栏约 28px（含最小化 / 最大化·还原 / 关闭）；无底部状态栏。

## 运行

```text
dist\VirtualScreen.exe
```

同目录需有 Qt DLL（`build_qt.ps1` 会用 windeployqt 部署）。

## 编译

依赖：Qt 5.14（msvc2017_64）+ VS2019。

```powershell
.\build_qt.ps1
```

## 操作

- 顶栏：**应用 / 清除 / 设置**，以及窗口按钮
- 双击标题栏：最大化 / 还原
- **虚拟屏 Tab**：切换预览；画面等比铺满（类似 VMware Fit）
- **预览:开/关**：可关掉抓屏

### 按项目配置虚拟屏

不同项目需要的虚拟屏数量和分辨率不同。在**设置**里可以：

- **添加虚拟屏 / 删除最后一块**（1～8 块）
- **配置方案下拉框**：一键切换 `dist\profiles\` 里已有配置
- **另存为… / 浏览…**：新建方案，或从任意路径打开 JSON
- **保存为当前**：写入启动默认的 `config.json`
- 顶栏 Tab 与当前配置一一对应（配置几块屏就有几个 Tab）

示例配置见 `dist\profiles\`（单屏 / 双屏 / 三屏）。切换配置后点**应用配置**才会真正创建虚拟显示器。若电脑变卡，请用**清除虚拟屏**禁用驱动（需管理员）；清除成功后虚拟屏应从「显示设置」里消失。

## 说明

- 依赖已安装的 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)
- 布局测试建议缩放 **100%**
