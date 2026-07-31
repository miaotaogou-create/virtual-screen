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

## 说明

- 依赖已安装的 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)
- 布局测试建议缩放 **100%**
