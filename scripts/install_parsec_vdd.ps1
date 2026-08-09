# 以管理员静默安装仓库捆绑的 Parsec VDD 驱动
[CmdletBinding()]
param(
    [string]$Installer = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $Installer) {
    $Installer = Join-Path $root "vendor\parsec-vdd\parsec-vdd-0.45.0.0.exe"
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "需要管理员权限，正在提权…"
    $arg = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    if ($Installer) { $arg += " -Installer `"$Installer`"" }
    Start-Process -FilePath "powershell.exe" -ArgumentList $arg -Verb RunAs -Wait
    exit $LASTEXITCODE
}

if (-not (Test-Path $Installer)) {
    throw "找不到安装包: $Installer"
}

Write-Host "安装: $Installer"
$p = Start-Process -FilePath $Installer -ArgumentList "/S" -Wait -PassThru
if ($p.ExitCode -ne 0 -and $null -ne $p.ExitCode) {
    # 部分安装器成功也返回非 0；再查设备
    Write-Host "安装器退出码: $($p.ExitCode)"
}

Start-Sleep -Seconds 2
$dev = Get-PnpDevice -Class Display -ErrorAction SilentlyContinue |
    Where-Object { $_.FriendlyName -match 'Parsec Virtual Display' }
if ($dev -and ($dev | Where-Object { $_.Status -eq 'OK' })) {
    Write-Host "OK: 已检测到 Parsec Virtual Display Adapter"
    exit 0
}

Write-Host "警告: 未检测到就绪的 Parsec 适配器，请打开设备管理器确认。"
exit 1
