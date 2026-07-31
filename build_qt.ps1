# 用 VS2019 + Qt5.14 msvc2017_64 编译 VirtualScreen
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$qmake = "C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin\qmake.exe"
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $qmake)) { throw "找不到 qmake: $qmake" }
if (-not (Test-Path $vcvars)) { throw "找不到 vcvars64: $vcvars" }

New-Item -ItemType Directory -Force -Path ".\build\qt",".\dist" | Out-Null
Get-Process VirtualScreen -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$cmd = @"
call "$vcvars"
cd /d "$PSScriptRoot"
"$qmake" VirtualScreen.pro -spec win32-msvc "CONFIG+=release"
if errorlevel 1 exit /b 1
nmake
if errorlevel 1 exit /b 1
"@
$bat = Join-Path $env:TEMP "build_virtualscreen_qt.bat"
Set-Content -Path $bat -Value $cmd -Encoding ASCII
cmd /c $bat
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

$exe = Join-Path $PSScriptRoot "dist\VirtualScreen.exe"
if (-not (Test-Path $exe)) { throw "未生成 $exe" }

# 部署 Qt 依赖（首次）
$windeploy = "C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin\windeployqt.exe"
& $windeploy --release --no-translations --no-angle --no-opengl-sw $exe

Write-Host "OK $exe"
