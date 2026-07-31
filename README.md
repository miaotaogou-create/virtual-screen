# virtual-screen

本机虚拟屏控制与预览工具（**Qt Widgets / C++**）。

中间预览区尽量铺满；标题栏约 28px；**无底部状态栏**（状态写在标题栏右侧）。

## 运行

编译产物在 `dist\`（含 Qt DLL）：

```text
dist\VirtualScreen.exe
```

双击运行。需要改分辨率/清虚拟屏时会提权（UAC）。

- **应用 / 清除 / 设置**：顶栏
- **虚拟屏 Tab**：切换预览目标；画面等比铺满中间区域（类似 VMware Fit）
- **预览:开/关**：可关掉抓屏以再省一点资源

## 编译

依赖：Qt 5.14（msvc2017_64）+ VS2019。

```powershell
.\build_qt.ps1
```

## 说明

- 虚拟显示依赖已安装的 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)
- 布局测试建议缩放 **100%**，否则逻辑分辨率变小，被测 UI 可能裁切
- 旧版 Python/tkinter 实现已不再维护，源码可参考仓库历史
