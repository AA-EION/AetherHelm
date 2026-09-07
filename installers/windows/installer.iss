; ==============================================================================
; AetherHelm Modern Inno Setup Installer Script
; ==============================================================================

#ifndef AppArch
  #define AppArch "x64"
#endif

#define MyAppName "AetherHelm"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AA-EION"
#define MyAppURL "https://github.com/AA-EION/AetherHelm"
#define MyAppExeName "AetherHelm.exe"

[Setup]
AppId={{D8B49B0E-1627-4B52-9CA3-6058097E9BF1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=AetherHelm-Windows-{#AppArch}-Setup
OutputDir=..\..\dist
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
#if AppArch == "arm64"
ArchitecturesInstallIn64BitMode=arm64
ArchitecturesAllowed=arm64
#else
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full Installation (Standalone, VST3 Plugin, MCP Server, Scripts)"
Name: "plugins_only"; Description: "Audio Plugins Only (VST3, CLAP)"
Name: "custom"; Description: "Custom Installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "AetherHelm Standalone Synthesizer Application"; Types: full custom
Name: "vst3"; Description: "VST3 Plugin (.vst3)"; Types: full plugins_only custom
Name: "clap"; Description: "CLAP Plugin (.clap)"; Types: full plugins_only custom
Name: "mcp"; Description: "Model Context Protocol (MCP) Server for AI Tools (Claude Desktop, Cursor, Windsurf, Cline)"; Types: full custom
Name: "presets"; Description: "Factory Patches & Presets"; Types: full plugins_only custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Components: standalone
Name: "configure_mcp"; Description: "Automatically configure Claude Desktop, Cursor, Windsurf, and Cline to connect to AetherHelm MCP Server"; GroupDescription: "AI Client Auto-Configuration:"; Components: mcp

[Files]
; Standalone Application
Source: "..\..\build\AetherHelm_artefacts\Release\Standalone\AetherHelm.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: standalone
Source: "..\..\dist\Standalone\AetherHelm.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: standalone
Source: "..\..\dist\AetherHelm.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: standalone

; VST3 Plugin to Common Files\VST3
Source: "..\..\build\AetherHelm_artefacts\Release\VST3\AetherHelm.vst3\*"; DestDir: "{commoncf64}\VST3\AetherHelm.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: vst3
Source: "..\..\dist\VST3\AetherHelm.vst3\*"; DestDir: "{commoncf64}\VST3\AetherHelm.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: vst3
Source: "..\..\dist\AetherHelm.vst3\*"; DestDir: "{commoncf64}\VST3\AetherHelm.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: vst3

; CLAP Plugin to Common Files\CLAP
Source: "..\..\build\AetherHelm_artefacts\Release\CLAP\AetherHelm.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion skipifsourcedoesntexist; Components: clap
Source: "..\..\dist\CLAP\AetherHelm.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion skipifsourcedoesntexist; Components: clap
Source: "..\..\dist\AetherHelm.clap"; DestDir: "{commoncf64}\CLAP"; Flags: ignoreversion skipifsourcedoesntexist; Components: clap

; MCP Server Executable
Source: "..\..\build\Release\aetherhelm-mcp.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: mcp
Source: "..\..\build\aetherhelm-mcp_artefacts\Release\aetherhelm-mcp.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: mcp
Source: "..\..\dist\aetherhelm-mcp.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist; Components: mcp

; MCP Management Scripts
Source: "..\..\scripts\install-mcp.ps1"; DestDir: "{app}\scripts"; Flags: ignoreversion; Components: mcp

; Factory Patches
Source: "..\..\patches\*"; DestDir: "{commonappdata}\AetherHelm\patches"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: presets
Source: "..\..\patches\*"; DestDir: "{commonappdata}\Helm\patches"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: presets
Source: "..\..\patches\*"; DestDir: "{commondocs}\AetherHelm\Patches"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: presets
Source: "..\..\patches\*"; DestDir: "{commondocs}\Helm\Patches"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: presets

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: standalone
Name: "{group}\AetherHelm MCP Server Configurator"; Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -File ""{app}\scripts\install-mcp.ps1"""; Components: mcp
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Components: standalone

[Run]
; Auto-register MCP Server with AI Desktop Clients if selected
Filename: "{app}\aetherhelm-mcp.exe"; Parameters: "--install"; StatusMsg: "Configuring desktop AI tools (Claude, Cursor, Windsurf, Cline)..."; Tasks: configure_mcp; Flags: runhidden
; Launch Standalone option on finish
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone
