# Build portable VirtualScreen.exe (same approach as qt-arm64-cross)
# Tcl refuses init.tcl under C:\WINDOWS\TEMP; launch.py copies tcl/tk to %LOCALAPPDATA%.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

Get-Process VirtualScreen -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item Env:TCL_LIBRARY -ErrorAction SilentlyContinue
Remove-Item Env:TK_LIBRARY -ErrorAction SilentlyContinue

$py = "C:\Users\49358\AppData\Local\Programs\Python\Python312\python.exe"
if (-not (Test-Path $py)) { $py = "python" }

& $py -c "import tkinter; r=tkinter.Tk(); print(r.tk.call('info','patchlevel')); r.destroy()"
if ($LASTEXITCODE -ne 0) { throw "tkinter unavailable" }

$pyRoot = & $py -c "import sys; print(sys.base_prefix)"
$pyRoot = "$pyRoot".Trim()
$tcl = Join-Path $pyRoot "tcl\tcl8.6"
$tk = Join-Path $pyRoot "tcl\tk8.6"
$tcl8 = Join-Path $pyRoot "tcl\tcl8"
if (-not (Test-Path (Join-Path $tcl "init.tcl"))) { throw "missing init.tcl: $tcl" }
if (-not (Test-Path (Join-Path $tk "tk.tcl"))) { throw "missing tk.tcl: $tk" }

if (Test-Path ".\build") { Remove-Item -Recurse -Force ".\build" }
if (Test-Path ".\dist\VirtualScreen.exe") { Remove-Item -Force ".\dist\VirtualScreen.exe" }
New-Item -ItemType Directory -Force -Path ".\dist" | Out-Null
Get-ChildItem -Filter "VirtualScreen.smoke_ok" -ErrorAction SilentlyContinue | Remove-Item -Force

$runPy = Join-Path $PSScriptRoot "launch.py"
$args = @(
  "-m", "PyInstaller",
  "--noconfirm", "--clean", "--onefile", "--windowed", "--noupx",
  "--name", "VirtualScreen",
  "--distpath", (Join-Path $PSScriptRoot "dist"),
  "--workpath", (Join-Path $PSScriptRoot "build\pyi"),
  "--specpath", (Join-Path $PSScriptRoot "build"),
  "--paths", $PSScriptRoot,
  "--add-data", ($tcl + ";_tcl_data"),
  "--add-data", ($tk + ";_tk_data")
)
if (Test-Path $tcl8) {
  $args += @("--add-data", ($tcl8 + ";tcl8"))
}
$args += $runPy

& $py -m pip install -q pyinstaller
& $py @args
if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed" }

$exe = Join-Path $PSScriptRoot "dist\VirtualScreen.exe"
if (-not (Test-Path $exe)) { throw "exe missing" }

function Invoke-Smoke([string]$ExePath, [string]$WorkDir) {
  $mark = Join-Path $WorkDir ((Split-Path $ExePath -Leaf) -replace '\.exe$', '.smoke_ok')
  if (Test-Path $mark) { Remove-Item -Force $mark }
  $p = Start-Process -FilePath $ExePath -ArgumentList "--smoke" -PassThru -Wait -WorkingDirectory $WorkDir
  if ($p.ExitCode -ne 0) { throw "smoke exit $($p.ExitCode)" }
  if (-not (Test-Path $mark)) { throw "smoke mark missing" }
  $txt = (Get-Content $mark -Raw).Trim()
  if ($txt -notmatch 'TK_OK') { throw "smoke bad: $txt" }
  if ($txt -match 'TCL_LIBRARY=.*\\WINDOWS\\TEMP\\') { throw "TCL still in TEMP: $txt" }
  Write-Host $txt
  Remove-Item -Force $mark -ErrorAction SilentlyContinue
}

Write-Host "=== smoke in dist ==="
Invoke-Smoke $exe (Join-Path $PSScriptRoot "dist")

Write-Host "=== portable smoke ==="
$tmp = Join-Path $env:TEMP ("vscreen-smoke-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory -Path $tmp | Out-Null
Copy-Item $exe (Join-Path $tmp "VirtualScreen.exe")
Invoke-Smoke (Join-Path $tmp "VirtualScreen.exe") $tmp

Write-Host "=== gui launch ==="
$g = Start-Process -FilePath $exe -PassThru -WorkingDirectory (Join-Path $PSScriptRoot "dist")
Start-Sleep -Seconds 4
if ($g.HasExited) { throw "GUI exited early code=$($g.ExitCode)" }
$title = ""
try { $title = (Get-Process -Id $g.Id).MainWindowTitle } catch {}
if ($title -match "Unhandled exception") {
  Stop-Process -Id $g.Id -Force
  throw "GUI error dialog"
}
Stop-Process -Id $g.Id -Force
Write-Host "GUI_ALIVE_OK"

try { Remove-Item -Recurse -Force $tmp -ErrorAction Stop } catch { Write-Host "cleanup tmp skipped" }
Write-Host ("OK " + $exe + " bytes=" + (Get-Item $exe).Length)
