# 准备单文件打包目录，并打开 Enigma Virtual Box（手工点几下即可）
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$evbGui = "C:\ZYL\tools\enigma-vb\app\enigmavb.exe"
if (-not (Test-Path $evbGui)) {
    $evbGui = "${env:ProgramFiles(x86)}\Enigma Virtual Box\enigmavb.exe"
}
$stage = Join-Path $PSScriptRoot "build\evb_stage"
$outDir = Join-Path $PSScriptRoot "dist\portable"
$dist = Join-Path $PSScriptRoot "dist"

Write-Host "== 编译 =="
& "$PSScriptRoot\build_qt.ps1"
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

Write-Host "== 准备 stage（Qt DLL + 插件 + VC 运行库） =="
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

# 清掉上次误拷进 portable 的散文件
Get-ChildItem $outDir -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# 配置与方案放 exe 旁边（不要打进 EVB），首启就能选预设
if (Test-Path "$dist\profiles") {
    Copy-Item "$dist\profiles" $outDir -Recurse -Force
}
$cfgSrc = if (Test-Path "$dist\config.example.json") { "$dist\config.example.json" } else { "$dist\config.json" }
if (Test-Path $cfgSrc) {
    Copy-Item $cfgSrc (Join-Path $outDir "config.json") -Force
}

$inputExe = Join-Path $stage "VirtualScreen.exe"
$outputExe = Join-Path $outDir "VirtualScreen.exe"

Write-Host ""
Write-Host "stage 已就绪。请在 Enigma Virtual Box 里按下面 5 步操作（约半分钟）："
Write-Host "  1. Input : $inputExe"
Write-Host "  2. Output: $outputExe"
Write-Host "  3. Add... → Add Folder Recursive → 选中目录："
Write-Host "       $stage"
Write-Host "     点「确定」"
Write-Host "  4. 若树里出现 VirtualScreen.exe，选中后点 Remove（主程序不必再虚拟一份）"
Write-Host "  5. 点 Process；完成后 File → Save Project As →"
Write-Host "       $(Join-Path $PSScriptRoot 'pack\VirtualScreen.evb')"
Write-Host ""
Write-Host "已预先放入 dist\portable\profiles 与 config.json（与 exe 同级，勿打进虚拟盒）。"
Write-Host ""

if (Test-Path $evbGui) {
    # 路径先放进剪贴板，方便粘贴
    Set-Clipboard -Value $stage
    Start-Process $evbGui
    Write-Host "已打开 EVB；stage 路径已复制到剪贴板。"
} else {
    Write-Host "未找到 enigmavb.exe，请先安装 Enigma Virtual Box。"
}
