# virtual-screen

一个绿色便携程序：**拷贝 `dist/VirtualScreen.exe` 到任意 Windows 机器双击即可**，无需安装 Python，无需 `.bat`。

用于在没有对应物理显示器时，造系统级虚拟屏（任意分辨率 / 缩放），并把画面缩小预览出来，方便测双屏 UI 布局。

## 怎么用

1. 双击 `VirtualScreen.exe`
2. 若尚未装驱动：点 **安装驱动**（弹 UAC → 确认）
3. 填两块屏的宽×高、缩放 → **应用**（会弹 UAC）
4. 把被测程序窗口拖到虚拟屏上，在本窗口看预览
5. 不用了点 **清除虚拟屏**

`config.json` 会生成在 **exe 同目录**（可一起拷走）。

## 说明

- 系统虚拟屏依赖上游 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)；exe 内已集成一键下载安装，不内嵌驱动二进制。
- 应用 / 清除 / 装驱动需要管理员（程序内自动弹 UAC）。
- 预览不需要管理员。
- 不是完整硬件仿真；DPI 偶发失败时可在系统「显示」里手动设缩放。

## 重新打包（维护用）

需本机 Python 3.12（带 Tcl/Tk）：

```text
scripts\build_exe.bat
```

输出仍是单个 `dist\VirtualScreen.exe`。
