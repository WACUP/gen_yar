; This is built assuming the following locations
;
;gen_yar\readme.txt
;gen_yar\source (source + lng_generator.exe)
;gen_yar\Language File\gen_yar.lng
;gen_yar\installer script\gen_yar.nsi


!define MINIMAL_VERSION "5.6.4.3418"

!define VERSION "1.12"
!define ALT_VER "1_12"
!define PLUG "Yar-matey! Playlist Copier"
!define PLUG_ALT "Yar-matey!_Playlist_Copier"
!define PLUG_FILE "gen_yar"

SetCompressor lzma
XPStyle on
AutoCloseWindow true
ShowInstDetails show

Name "${PLUG} v${VERSION}"
OutFile "${PLUG_ALT}_v${ALT_VER}.exe"

InstType "Plugin only"
InstType "Plugin + language file"
InstType "Plugin + source files"
InstType "Everything"
InstType /NOCUSTOM
InstType /COMPONENTSONLYONCUSTOM

;Header Files

!include "WordFunc.nsh"
!include "Sections.nsh"
!include "WinMessages.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!insertmacro GetSize

InstallDir $PROGRAMFILES\Winamp
InstProgressFlags smooth

InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Winamp" "UninstallString"

DirText "Please select your Winamp path below (you will be able to proceed when Winamp is detected):"

;--------------------------------

PageEx directory
PageCallbacks "" "" directoryLeave
Caption " "
PageExEnd
Page components
Page instfiles

;--------------------------------

Section ""
  SetOverwrite on
  SetOutPath "$INSTDIR\Plugins"
  File "..\Plugin\${PLUG_FILE}.dll"
  SetOverwrite off
SectionEnd

Section "Example language file"
  SectionIn 2 4
  SetOverwrite on
  SetOutPath "$INSTDIR\Plugins\${PLUG_FILE}\Language File"
  File "..\Language File\${PLUG_FILE}.lng"
  SetOverwrite off
SectionEnd

Section "Source files"
  SectionIn 3 4
  SetOverwrite on
  SetOutPath "$INSTDIR\Plugins\${PLUG_FILE}"
  File "..\readme.txt"

  SetOutPath "$INSTDIR\Plugins\${PLUG_FILE}\Source"
  File "..\Source\*"

  SetOutPath "$INSTDIR\Plugins\${PLUG_FILE}\Installer Script"
  File "..\Installer Script\${PLUG_FILE}.nsi"
  SetOverwrite off
SectionEnd

Function .onInit
  ;Detect running Winamp instances and close them
  !define WINAMP_FILE_EXIT 40001

  FindWindow $R0 "Winamp v1.x"
  IntCmp $R0 0 ok
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "Please close all instances of Winamp before installing$\n\
					    ${PLUG} v${VERSION}.$\nAttempt to close Winamp now?" IDYES checkagain IDNO no
    checkagain:
      FindWindow $R0 "Winamp v1.x"
      IntCmp $R0 0 ok
      SendMessage $R0 ${WM_COMMAND} ${WINAMP_FILE_EXIT} 0
      Goto checkagain
    no:
       Abort
  ok:
FunctionEnd

Function directoryLeave
  Call CheckWinampVersion
FunctionEnd

Function .onInstSuccess
  MessageBox MB_YESNO '${PLUG} was installed.$\nDo you want to run Winamp now?' /SD IDYES IDNO end
    ExecShell open "$INSTDIR\Winamp.exe"
  end:
FunctionEnd

Function .onVerifyInstDir
  ;Check for Winamp installation
  IfFileExists $INSTDIR\Winamp.exe Good
    Abort
  Good:
FunctionEnd

Function CheckWinampVersion
  ${GetFileVersion} "$INSTDIR\winamp.exe" $R0 ; Get Winamp.exe version information, $R0 = Actual Version
  ${if} $R0 != "" ; check if Version info is not empty
    ${VersionCompare} $R0 ${MINIMAL_VERSION} $R1 ; $R1 = Result $R1=0  Versions are equal, $R1=1  Version1 is newer, $R1=2  Version2 is newer
    ${if} $R1 == "2"
      MessageBox MB_OK "Warning: This requires at least Winamp v${MINIMAL_VERSION} or higher to continue.$\n$\nPlease update your Winamp install if you are using an older version or check$\nto see if there is an updated version of ${PLUG} for your install.$\n$\nThe detected version of your Winamp install is: $R0"
      Abort
    ${EndIf}
  ${Else}
    MessageBox MB_OK "Warning: A valid Winamp install was not detected in the specified path.$\n$\nPlease check the Winamp directory and either install the latest version$\nfrom Winamp.com or choose another directory with a valid Winamp install$\nbefore you can install the ${PLUG} on your machine."
    Abort
  ${EndIf}
FunctionEnd