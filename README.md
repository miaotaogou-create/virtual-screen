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

- 改显示配置 / 装驱动需要管理员。程序**启动时提权一次**（弹一次 UAC），之后本会话内应用/清除一般不再反复弹窗。
- Windows **不能**让程序静默自动点同意 UAC；若需完全不弹，只能在系统里关闭 UAC（不推荐）或以管理员账户长期登录。
