; magic.RIDE Inno Setup Script
; Creates a Windows installer for the plugin

#define MyAppName "magic.RIDE"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "MBM Audio"
#define MyAppURL "https://mbmaudio.com"

[Setup]
AppId={{8A7B5C3D-4E2F-1A0B-9C8D-7E6F5A4B3C2D}
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
Source: "..\..\build\VocalRider_artefacts\Release\VST3\{#MyAppName}.vst3\*"; DestDir: "{commonpf64}\Common Files\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; AAX Plugin (for Pro Tools)
Source: "..\..\build\VocalRider_artefacts\Release\AAX\{#MyAppName}.aaxplugin\*"; DestDir: "{commonpf64}\Avid\Audio\Plug-Ins\{#MyAppName}.aaxplugin"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: AAXSourceExists

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel1=Welcome to the {#MyAppName} Setup Wizard
WelcomeLabel2=This will install {#MyAppName} {#MyAppVersion} on your computer.%n%nPlugins will be installed to:%n  VST3: C:\Program Files\Common Files\VST3\%n  AAX:  C:\Program Files\Common Files\Avid\Audio\Plug-Ins\%n%nClick Next to continue.
FinishedLabel=Setup has finished installing {#MyAppName} on your computer.%n%nOpen your DAW (Ableton, FL Studio, Pro Tools, etc.) and rescan plugins to see {#MyAppName}.

[Code]
function AAXSourceExists(): Boolean;
begin
  Result := DirExists(ExpandConstant('{src}\..\..\build\VocalRider_artefacts\Release\AAX\{#MyAppName}.aaxplugin'));
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
end;
