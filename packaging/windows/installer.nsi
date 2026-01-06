; ═══════════════════════════════════════════════════════════════════════════════
; UFT Windows Installer - NSIS Script
; ═══════════════════════════════════════════════════════════════════════════════

!ifndef VERSION
  !define VERSION "3.3.0"
!endif

!define APPNAME "UnifiedFloppyTool"
!define COMPANYNAME "UFT Project"
!define DESCRIPTION "Professional Floppy Disk Preservation Tool"
!define INSTALLSIZE 150000

; Modern UI
!include "MUI2.nsh"
!include "FileFunc.nsh"

; General
Name "${APPNAME} ${VERSION}"
OutFile "UFT-${VERSION}-Windows-x64-Setup.exe"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin

; UI Settings
!define MUI_ICON "..\..\resources\icons\app_icon.ico"
!define MUI_UNICON "..\..\resources\icons\app_icon.ico"
!define MUI_HEADERIMAGE
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"

; ═══════════════════════════════════════════════════════════════════════════════
; Installation
; ═══════════════════════════════════════════════════════════════════════════════

Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Main application and Qt dependencies
    File /r "..\..\deploy\windows\*.*"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Start Menu shortcuts
    CreateDirectory "$SMPROGRAMS\${APPNAME}"
    CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\UnifiedFloppyTool.exe"
    CreateShortCut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    
    ; Desktop shortcut (optional)
    CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\UnifiedFloppyTool.exe"
    
    ; Registry entries for Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                     "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                     "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                     "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                     "DisplayIcon" "$INSTDIR\UnifiedFloppyTool.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                     "Publisher" "${COMPANYNAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                     "DisplayVersion" "${VERSION}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                      "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                      "NoRepair" 1
    
    ; Estimated size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" \
                      "EstimatedSize" "$0"
    
    ; File associations (optional)
    WriteRegStr HKCR ".adf" "" "UFT.DiskImage"
    WriteRegStr HKCR ".d64" "" "UFT.DiskImage"
    WriteRegStr HKCR ".g64" "" "UFT.DiskImage"
    WriteRegStr HKCR ".scp" "" "UFT.FluxImage"
    WriteRegStr HKCR ".hfe" "" "UFT.FluxImage"
    WriteRegStr HKCR "UFT.DiskImage" "" "Disk Image"
    WriteRegStr HKCR "UFT.DiskImage\DefaultIcon" "" "$INSTDIR\UnifiedFloppyTool.exe,0"
    WriteRegStr HKCR "UFT.DiskImage\shell\open\command" "" '"$INSTDIR\UnifiedFloppyTool.exe" "%1"'
    WriteRegStr HKCR "UFT.FluxImage" "" "Flux Image"
    WriteRegStr HKCR "UFT.FluxImage\DefaultIcon" "" "$INSTDIR\UnifiedFloppyTool.exe,0"
    WriteRegStr HKCR "UFT.FluxImage\shell\open\command" "" '"$INSTDIR\UnifiedFloppyTool.exe" "%1"'
    
SectionEnd

; ═══════════════════════════════════════════════════════════════════════════════
; Uninstallation
; ═══════════════════════════════════════════════════════════════════════════════

Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR"
    
    ; Remove shortcuts
    RMDir /r "$SMPROGRAMS\${APPNAME}"
    Delete "$DESKTOP\${APPNAME}.lnk"
    
    ; Remove registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
    DeleteRegKey HKLM "Software\${APPNAME}"
    
    ; Remove file associations
    DeleteRegKey HKCR "UFT.DiskImage"
    DeleteRegKey HKCR "UFT.FluxImage"
    DeleteRegValue HKCR ".adf" ""
    DeleteRegValue HKCR ".d64" ""
    DeleteRegValue HKCR ".g64" ""
    DeleteRegValue HKCR ".scp" ""
    DeleteRegValue HKCR ".hfe" ""
    
SectionEnd
