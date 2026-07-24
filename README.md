# virtual-screen

**原生窗口绿色单文件**：拷贝 `dist\VirtualScreen.exe` 到任意目录双击即可（不装 Python）。

做法对齐 `qt-arm64-cross`：PyInstaller `--onefile`；Tcl/Tk 首次运行拷到 `%LOCALAPPDATA%\VirtualScreen\`，避开 `C:\WINDOWS\TEMP` 下 Tcl 拒读 `init.tcl`。

## 怎么用

1. 双击 `VirtualScreen.exe`（原生 GUI，不是网页）
2. 未装驱动 → **安装驱动**（弹 UAC）
3. 填分辨率/缩放 → **应用**（弹 UAC）
4. 把被测程序拖到虚拟屏，看窗口内预览
5. **清除虚拟屏**

`config.json` 写在 exe 同目录。

## 重新打包

```powershell
pip install pyinstaller
.\build_exe.ps1
```

脚本会做：本目录 smoke、空目录 smoke、GUI 存活自检。

## 说明

- 系统虚拟屏依赖 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)；程序内一键安装/应用/清除。
- 不是完整硬件仿真。
