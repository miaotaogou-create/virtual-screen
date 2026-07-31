# 用 Enigma Virtual Box 打成单文件绿色包
# 依赖：已安装 EVB（本机默认 C:\ZYL\tools\enigma-vb\app）
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$evbConsole = "C:\ZYL\tools\enigma-vb\app\enigmavbconsole.exe"
$evbGui = "C:\ZYL\tools\enigma-vb\app\enigmavb.exe"
$project = Join-Path $PSScriptRoot "pack\VirtualScreen.evb"
$stage = Join-Path $PSScriptRoot "build\evb_stage"
$outDir = Join-Path $PSScriptRoot "dist\portable"
$dist = Join-Path $PSScriptRoot "dist"

Write-Host "== 1) 先正常编译并 windeployqt =="
& "$PSScriptRoot\build_qt.ps1"
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

Write-Host "== 2) 准备打包目录（不含 config/profiles，便于旁边可写） =="
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage, $outDir | Out-Null
Copy-Item "$dist\VirtualScreen.exe" $stage
Copy-Item "$dist\*.dll" $stage -ErrorAction SilentlyContinue
foreach ($d in "platforms", "imageformats", "iconengines", "styles") {
    if (Test-Path "$dist\$d") { Copy-Item "$dist\$d" $stage -Recurse -Force }
}
foreach ($f in "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll") {
    $src = "C:\Windows\System32\$f"
    if (Test-Path $src) { Copy-Item $src $stage -Force }
}

$inputExe = Join-Path $stage "VirtualScreen.exe"
$outputExe = Join-Path $outDir "VirtualScreen.exe"

if (Test-Path $project) {
    Write-Host "== 3) 用已保存的 .evb 工程打包 =="
    & $evbConsole $project -input $inputExe -output $outputExe
    if ($LASTEXITCODE -ne 0) { throw "enigmavbconsole 失败" }
} else {
    Write-Host @"

还没有 pack\VirtualScreen.evb。请在 Enigma Virtual Box 里做一次：

1. Enter Input File Name:
   $inputExe
2. Enter Output File Name:
   $outputExe
3. 点 Add...，把下面这些加进 Virtual Box Files（不要重复加 exe 自己）：
   - $stage\*.dll
   - $stage\platforms
   - $stage\imageformats
   - $stage\iconengines
   - $stage\styles
   添加时选「保持相对目录 / Recurse」类选项，保证 platforms\qwindows.dll 路径正确。
4. Files Options → 勾选压缩（可选）
5. 点 Process
6. 菜单 File → Save Project As…
   保存为：$project
   以后再跑本脚本就会自动打包。

"@
    if (Test-Path $evbGui) { Start-Process $evbGui }
    exit 0
}

# 旁边放配置模板，单文件 exe 读写真实磁盘上的这些文件
Copy-Item "$dist\config.example.json" $outDir -Force -ErrorAction SilentlyContinue
if (Test-Path "$dist\profiles") {
    Copy-Item "$dist\profiles" $outDir -Recurse -Force
}
if (-not (Test-Path "$outDir\config.json") -and (Test-Path "$dist\config.example.json")) {
    Copy-Item "$dist\config.example.json" "$outDir\config.json"
}

Write-Host "OK 单文件包: $outputExe"
Get-Item $outputExe | Format-List FullName, Length, LastWriteTime
