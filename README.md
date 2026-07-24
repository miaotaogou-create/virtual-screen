# virtual-screen

**原生窗口绿色单文件**：拷贝 `dist\VirtualScreen.exe` 到任意目录双击即可（不装 Python）。

做法对齐 `qt-arm64-cross`：PyInstaller `--onefile`；Tcl/Tk 首次运行拷到 `%LOCALAPPDATA%\VirtualScreen\`，避开 `C:\WINDOWS\TEMP` 下 Tcl 拒读 `init.tcl`。

## 怎么用

1. 双击 `VirtualScreen.exe`（原生 GUI）
2. 主界面以**预览为主**；点右上角 **设置** 再改分辨率/缩放、装驱动
3. **应用** 后把被测程序拖到虚拟屏，在大预览区观察
4. **清除** 可关掉虚拟屏

顶栏可直接「应用 / 清除」；参数不常驻主画面，避免挤占预览。

## 重新打包

```powershell
pip install pyinstaller
.\build_exe.ps1
```

脚本会做：本目录 smoke、空目录 smoke、GUI 存活自检。

## 说明

- 系统虚拟屏依赖 [Virtual Display Driver](https://github.com/VirtualDrivers/Virtual-Display-Driver)；程序内一键安装/应用/清除。
- 不是完整硬件仿真。
