# Build + NSIS setup (app + silent Parsec VDD)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$makensis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"
if (-not (Test-Path $makensis)) {
    $makensis = "$env:ProgramFiles\NSIS\makensis.exe"
}
if (-not (Test-Path $makensis)) {
    throw "makensis.exe not found; install NSIS 3"
}

Write-Host "== build =="
& "$PSScriptRoot\build_qt.ps1"
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$need = @(
    "dist\VirtualScreen.exe",
    "dist\Qt5Core.dll", "dist\Qt5Gui.dll", "dist\Qt5Svg.dll", "dist\Qt5Widgets.dll",
    "dist\msvcp140.dll", "dist\vcruntime140.dll", "dist\vcruntime140_1.dll",
    "dist\style.qss", "dist\config.example.json",
    "dist\platforms", "dist\styles", "dist\imageformats", "dist\iconengines",
    "dist\profiles",
    "vendor\parsec-vdd\parsec-vdd-0.45.0.0.exe"
)
foreach ($p in $need) {
    if (-not (Test-Path (Join-Path $PSScriptRoot $p))) {
        throw "missing: $p"
    }
}

$out = Join-Path $PSScriptRoot "dist\VirtualScreen-Setup-v1.1.exe"
Remove-Item -Force $out -ErrorAction SilentlyContinue

Write-Host "== nsis =="
& $makensis "/V2" (Join-Path $PSScriptRoot "pack\VirtualScreen.nsi")
if ($LASTEXITCODE -ne 0) { throw "nsis failed" }
if (-not (Test-Path $out)) { throw "missing output: $out" }

Get-Item $out | Format-List FullName, Length, LastWriteTime
Write-Host "OK $out"
