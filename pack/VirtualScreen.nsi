; VirtualScreen setup: app + silent Parsec VDD
Unicode true
!define PRODUCT_NAME "VirtualScreen"
!define PRODUCT_VERSION "1.1.0"
!define PRODUCT_PUBLISHER "VirtualScreen"
!define PRODUCT_WEB "https://github.com/miaotaogou-create/virtual-screen"
!define DRIVER_SETUP "parsec-vdd-0.45.0.0.exe"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\dist\VirtualScreen-Setup-v1.1.exe"
InstallDir "$PROGRAMFILES64\VirtualScreen"
InstallDirRegKey HKLM "Software\VirtualScreen" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show

!include "MUI2.nsh"
!include "LogicLib.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\resources\VirtualScreen.ico"
!define MUI_UNICON "..\resources\VirtualScreen.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\VirtualScreen.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run VirtualScreen"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "FileDescription" "VirtualScreen Setup"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "${PRODUCT_PUBLISHER}"

Section "App" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  File "..\dist\VirtualScreen.exe"
  File "..\dist\Qt5Core.dll"
  File "..\dist\Qt5Gui.dll"
  File "..\dist\Qt5Svg.dll"
  File "..\dist\Qt5Widgets.dll"
  File "..\dist\msvcp140.dll"
  File "..\dist\vcruntime140.dll"
  File "..\dist\vcruntime140_1.dll"
  File "..\dist\style.qss"
  File "..\dist\config.example.json"

  ${IfNot} ${FileExists} "$INSTDIR\config.json"
    File "/oname=config.json" "..\dist\config.example.json"
  ${EndIf}

  SetOutPath "$INSTDIR\platforms"
  File /r "..\dist\platforms\*.*"
  SetOutPath "$INSTDIR\styles"
  File /r "..\dist\styles\*.*"
  SetOutPath "$INSTDIR\imageformats"
  File /r "..\dist\imageformats\*.*"
  SetOutPath "$INSTDIR\iconengines"
  File /r "..\dist\iconengines\*.*"

  SetOutPath "$INSTDIR\profiles"
  File /r "..\dist\profiles\*.*"

  SetOutPath "$INSTDIR\parsec-vdd"
  File "..\vendor\parsec-vdd\${DRIVER_SETUP}"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\VirtualScreen" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "URLInfoAbout" "${PRODUCT_WEB}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "DisplayIcon" "$INSTDIR\VirtualScreen.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\VirtualScreen"
  CreateShortCut "$SMPROGRAMS\VirtualScreen\VirtualScreen.lnk" "$INSTDIR\VirtualScreen.exe"
  CreateShortCut "$SMPROGRAMS\VirtualScreen\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortCut "$DESKTOP\VirtualScreen.lnk" "$INSTDIR\VirtualScreen.exe"
SectionEnd

Section "Parsec VDD" SecDriver
  SectionIn RO
  DetailPrint "Installing Parsec VDD (silent)..."
  ClearErrors
  ExecWait '"$INSTDIR\parsec-vdd\${DRIVER_SETUP}" /S' $0
  DetailPrint "Driver setup exit code: $0"
SectionEnd

Section "Uninstall"
  ; Keep Parsec VDD on the system
  Delete "$DESKTOP\VirtualScreen.lnk"
  Delete "$SMPROGRAMS\VirtualScreen\VirtualScreen.lnk"
  Delete "$SMPROGRAMS\VirtualScreen\Uninstall.lnk"
  RMDir "$SMPROGRAMS\VirtualScreen"

  Delete "$INSTDIR\VirtualScreen.exe"
  Delete "$INSTDIR\Qt5Core.dll"
  Delete "$INSTDIR\Qt5Gui.dll"
  Delete "$INSTDIR\Qt5Svg.dll"
  Delete "$INSTDIR\Qt5Widgets.dll"
  Delete "$INSTDIR\msvcp140.dll"
  Delete "$INSTDIR\vcruntime140.dll"
  Delete "$INSTDIR\vcruntime140_1.dll"
  Delete "$INSTDIR\style.qss"
  Delete "$INSTDIR\config.example.json"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\iconengines"
  RMDir /r "$INSTDIR\profiles"
  RMDir /r "$INSTDIR\parsec-vdd"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VirtualScreen"
  DeleteRegKey HKLM "Software\VirtualScreen"
SectionEnd