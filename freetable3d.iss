; Script Inno Setup (https://jrsoftware.org/isinfo.php) : installateur Windows de FreeTable3D.
; Prérequis : avoir fabriqué le dossier « dist » avec packaging/windows/deployer.sh.
; Compilation : ouvrir ce fichier dans Inno Setup, puis Build > Compile.
; Résultat : installateur\FreeTable3D-<version>-windows-setup.exe

#define AppName "FreeTable3D"
#define AppVersion "0.1.4"

[Setup]
AppId={{B3A5F2D0-7C1E-4D1A-9F3B-6E2A1C0D4F57}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=KREUDER Olivier
AppPublisherURL=https://github.com/morvux/freetable3d
AppSupportURL=https://github.com/morvux/freetable3d/issues
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
LicenseFile=..\..\LICENSE
OutputDir=..\..\installateur
OutputBaseFilename=FreeTable3D-{#AppVersion}-windows-setup
SetupIconFile=freetable3d.ico
UninstallDisplayIcon={app}\freetable3d.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\freetable3d.exe"; IconFilename: "{app}\freetable3d.ico"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\freetable3d.exe"; IconFilename: "{app}\freetable3d.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\freetable3d.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
