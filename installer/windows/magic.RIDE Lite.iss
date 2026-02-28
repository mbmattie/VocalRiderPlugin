; magic.RIDE Lite Inno Setup Script
; Creates a Windows installer for the free Lite plugin

#define MyAppName "magic.RIDE Lite"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "MBM Audio"
#define MyAppURL "https://mbmaudio.com"

[Setup]
AppId={{F1A2B3C4-D5E6-7F89-0A1B-2C3D4E5F6A7B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={commonpf64}\Common Files\VST3
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\output
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
DisableDirPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; VST3 Plugin
Source: "..\..\build\VocalRiderLite_artefacts\Release\VST3\{#MyAppName}.vst3\*"; DestDir: "{commonpf64}\Common Files\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel1=Welcome to the {#MyAppName} Setup Wizard
WelcomeLabel2=This will install {#MyAppName} {#MyAppVersion} on your computer.%n%nPlugins will be installed to:%n  VST3: C:\Program Files\Common Files\VST3\%n%nClick Next to continue.
FinishedLabel=Setup has finished installing {#MyAppName} on your computer.%n%nOpen your DAW (Ableton, FL Studio, etc.) and rescan plugins to see {#MyAppName}.

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
