# 兼容旧入口：转发到 Parsec VDD 安装脚本
& "$PSScriptRoot\install_parsec_vdd.ps1" @args
exit $LASTEXITCODE
