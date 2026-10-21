[Setup]
AppId={{6B1F2C4A-9D3E-4A57-8C21-5E7A0D9B4F13}
AppName=CompSat
AppVersion=1.0.0
AppPublisher=YourName
DefaultDirName={commoncf64}\VST3\CompSat.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=CompSat-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\build\CompSat_artefacts\Release\VST3\CompSat.vst3\*"; DestDir: "{commoncf64}\VST3\CompSat.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
