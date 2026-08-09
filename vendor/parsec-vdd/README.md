# Parsec Virtual Display Driver（捆绑包）

本目录存放 **Parsec VDD 官方驱动静默安装包**，便于克隆本仓库后离线/一键安装，无需再去别处找驱动。

| 文件 | 说明 |
|------|------|
| `parsec-vdd-0.45.0.0.exe` | 官方驱动安装程序（约 500KB） |

- **来源**：https://builds.parsec.app/vdd/parsec-vdd-0.45.0.0.exe  
  （与 [nomi-san/parsec-vdd](https://github.com/nomi-san/parsec-vdd) 文档一致）
- **版本**：0.45.0.0
- **SHA256**：`E23332448FDAF5AA017CB308DB5EF6855FAC526A7DED05D80C039404126D5362`
- **驱动版权**：归 Parsec 所有；本仓库仅原样转发安装包，便于部署。控制/预览逻辑见本项目代码。

## 安装

管理员 PowerShell：

```powershell
.\scripts\install_parsec_vdd.ps1
```

或：

```text
vendor\parsec-vdd\parsec-vdd-0.45.0.0.exe /S
```

装好后设备管理器应出现「Parsec Virtual Display Adapter」。  
日常加屏/预览用本仓库的 `VirtualScreen.exe`；不必再开官方 ParsecVDisplay。
