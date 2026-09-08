#define MyAppName "Nugz Support Tool"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Nugz"
#define MyAppExeName "Support.exe"
#define MyKeyGenExeName "KeyGen.exe"

[Setup]
AppId={{D6A64C3F-CC2F-4FE1-9BF4-6A5E4D4E2F73}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Nugz Support Tool
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=no
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=NugzSupportTool-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
Source: "Support.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "KeyGen.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Nugz Support Tool"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\Nugz Key Generator"; Filename: "{app}\{#MyKeyGenExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\Nugz Support Tool"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Nugz Support Tool"; Flags: nowait postinstall skipifsilent
