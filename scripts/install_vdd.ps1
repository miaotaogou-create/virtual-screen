# 以管理员静默安装 Virtual Display Driver（IddCx / MttVDD）
# 逻辑对齐上游 Community Scripts/silent-install.ps1
[CmdletBinding()]
param(
    [string]$NefConURL = "https://github.com/nefarius/nefcon/releases/download/v1.14.0/nefcon_v1.14.0.zip",
    [string]$DriverURL = "https://github.com/VirtualDrivers/Virtual-Display-Driver/releases/download/25.7.23/VirtualDisplayDriver-x86.Driver.Only.zip",
    [string]$LogPath = ""
)

$ErrorActionPreference = "Stop"
if (-not $LogPath) {
    $LogPath = Join-Path $env:TEMP "vdd-install.log"
}

function Write-Log([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    Add-Content -Path $LogPath -Value $line -Encoding UTF8
    Write-Host $line
}

try {
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        throw "需要管理员权限"
    }

    Write-Log "开始安装 VDD，日志: $LogPath"
    $tempDir = Join-Path $env:TEMP ("VDDInstall_" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    Write-Log "下载 NefCon..."
    $NefConZipPath = Join-Path $tempDir "nefcon.zip"
    Invoke-WebRequest -Uri $NefConURL -OutFile $NefConZipPath -UseBasicParsing
    Expand-Archive -Path $NefConZipPath -DestinationPath $tempDir -Force
    $NefConExe = Join-Path $tempDir "x64\nefconw.exe"
    if (-not (Test-Path $NefConExe)) { throw "找不到 nefconw.exe" }

    Write-Log "下载驱动包..."
    $driverZipPath = Join-Path $tempDir "driver.zip"
    Invoke-WebRequest -Uri $DriverURL -OutFile $driverZipPath -UseBasicParsing
    Expand-Archive -Path $driverZipPath -DestinationPath $tempDir -Force

    $inf = Get-ChildItem -Path $tempDir -Recurse -Filter "MttVDD.inf" | Select-Object -First 1
    if (-not $inf) { throw "压缩包内找不到 MttVDD.inf" }
    $driverDir = $inf.Directory.FullName
    Write-Log "驱动目录: $driverDir"

    $catFile = Get-ChildItem -Path $driverDir -Filter "*.cat" | Select-Object -First 1
    if ($catFile) {
        Write-Log "导入驱动证书: $($catFile.FullName)"
        $catBytes = [System.IO.File]::ReadAllBytes($catFile.FullName)
        $certificates = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2Collection
        $certificates.Import($catBytes)
        $certsFolder = Join-Path $tempDir "ExportedCerts"
        New-Item -ItemType Directory -Path $certsFolder -Force | Out-Null
        foreach ($cert in $certificates) {
            $certFilePath = Join-Path $certsFolder "$($cert.Thumbprint).cer"
            [System.IO.File]::WriteAllBytes($certFilePath, $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert))
            Import-Certificate -FilePath $certFilePath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
        }
    }

    Write-Log "nefcon 安装 Root\MttVDD ..."
    Push-Location $driverDir
    try {
        & $NefConExe install .\MttVDD.inf "Root\MttVDD"
        $rc = $LASTEXITCODE
        Write-Log "nefcon 退出码: $rc"
    } finally {
        Pop-Location
    }

    Start-Sleep -Seconds 8

    # 确保配置目录存在
    $cfgDir = "C:\VirtualDisplayDriver"
    if (-not (Test-Path $cfgDir)) {
        New-Item -ItemType Directory -Path $cfgDir -Force | Out-Null
        Write-Log "已创建 $cfgDir"
    }

    Write-Log "安装流程结束"
    Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    exit 0
}
catch {
    Write-Log ("失败: " + $_.Exception.Message)
    exit 1
}
