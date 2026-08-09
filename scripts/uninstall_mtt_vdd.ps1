# 卸载旧版 MTT / VirtualDrivers「Virtual Display Driver」（不影响 Parsec VDD）
[CmdletBinding()]
param()

$ErrorActionPreference = "Continue"
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "需要管理员，正在提权…"
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs -Wait
    exit $LASTEXITCODE
}

Write-Host "== 查找 MTT / 非 Parsec 的 Virtual Display 设备 =="
$devs = Get-PnpDevice -Class Display -ErrorAction SilentlyContinue | Where-Object {
    ($_.FriendlyName -match 'Virtual Display Driver|MttVDD|IddSample') -and
    ($_.FriendlyName -notmatch 'Parsec')
}
if (-not $devs) {
    Write-Host "未找到 MTT 类设备（可能已卸干净）。"
} else {
    foreach ($d in $devs) {
        Write-Host "移除设备: $($d.FriendlyName)  $($d.InstanceId)"
        pnputil /remove-device $d.InstanceId 2>&1 | Write-Host
        Disable-PnpDevice -InstanceId $d.InstanceId -Confirm:$false -ErrorAction SilentlyContinue
    }
}

Write-Host "== 查找并删除驱动包（排除 Parsec） =="
$raw = pnputil /enum-drivers 2>&1 | Out-String
$blocks = $raw -split "(?=Published Name:|已发布名称:|发布名称:)"
foreach ($b in $blocks) {
    if ($b -match 'Parsec') { continue }
    $isMtt = $b -match 'MttVDD|Mtt VDD|MikeTheTech|VirtualDrivers|IddSample|Virtual Display Driver'
    if (-not $isMtt) { continue }
    if ($b -match '(oem\d+\.inf)') {
        $oem = $Matches[1]
        Write-Host "删除驱动包: $oem"
        pnputil /delete-driver $oem /uninstall /force 2>&1 | Write-Host
    }
}

Write-Host "== 清理目录 =="
@(
    'C:\VirtualDisplayDriver',
    "$env:ProgramFiles\Virtual Display Driver",
    "$env:ProgramFiles\VirtualDisplayDriver",
    "${env:ProgramFiles(x86)}\Virtual Display Driver"
) | ForEach-Object {
    if (Test-Path $_) {
        Write-Host "删除 $_"
        Remove-Item $_ -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "== 结果 =="
Get-PnpDevice -Class Display -ErrorAction SilentlyContinue |
    Where-Object { $_.FriendlyName -match 'Virtual|Parsec|Mtt' } |
    Format-Table Status, FriendlyName, InstanceId -AutoSize
Write-Host "完成。若仍看到 MTT 设备，可重启后再跑一次本脚本。"
