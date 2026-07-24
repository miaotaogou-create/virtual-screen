# virtual-screen

**一个绿色 `VirtualScreen.exe`，拷走双击即可**（不装 Python、不用 bat、不依赖 Tcl/Tk）。

打开后自动弹出浏览器本地页，用来配置虚拟屏分辨率/缩放、安装驱动、预览画面。

## 怎么用

1. 双击 `dist\VirtualScreen.exe`（浏览器会打开控制页）
2. 未装驱动时点 **安装驱动**（弹 UAC）
3. 填宽×高、缩放 → **应用**（弹 UAC）
4. 把被测程序拖到虚拟屏，在页面里看预览
5. **清除虚拟屏** / **退出程序**

`config.json` 生成在 **exe 同目录**。

## 说明

- 系统虚拟屏仍用上游 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)；安装/应用/清除在程序内完成并自动提权。
- UI 是本机 `127.0.0.1` 网页（标准库 HTTP），打包不带 tkinter，避免 Tcl 绿色包问题。
- 不是完整硬件仿真。

## 重打包

```text
scripts\build_exe.bat
```
