[Setup]
AppName=Mach1 Spatial System
AppVersion=2.0.0
AppPublisher=Mach1
DefaultDirName={pf64}\Mach1 Spatial System
DisableProgramGroupPage=yes
VersionInfoVersion=2.0.0
VersionInfoDescription=
SetupIconFile=data\mach1logo.ico
OutputDir=Output
OutputBaseFilename=Mach1 Spatial System Installer
LicenseFile=..\License\license.rtf
InfoBeforeFile=..\intro.rtf
InfoAfterFile=..\summary.rtf
DisableWelcomePage=no
DefaultGroupName=Mach1
Uninstallable=yes
UninstallDisplayName=Mach1 Spatial System Uninstaller

[Types]
Name: full; Description: Full installation
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: aax; Description: AAX; Types: full
Name: vst2; Description: VST2; Types: full
//Name: vst3; Description: VST3
Name: m1player; Description: M1VideoPlayer; Types: full
Name: m1transcoder; Description: M1Transcoder; Types: full custom; Flags: fixed
Name: m1services; Description: M1Services; Types: full custom; Flags: fixed

[UninstallDelete]
Name: {app}; Type: filesandordirs

[Files]
Source: "..\..\m1-monitor\build\M1-Monitor_artefacts\Release\AAX\M1-Monitor.aaxplugin"; DestDir: "{pf64}\Common Files\Avid\Audio\Plug-Ins"; Components: aax; Flags: ignoreversion recursesubdirs
Source: "..\..\m1-panner\build\M1-Panner_artefacts\Release\AAX\M1-Panner.aaxplugin"; DestDir: "{pf64}\Common Files\Avid\Audio\Plug-Ins"; Components: aax; Flags: ignoreversion recursesubdirs
Source: "..\resources\templates\PTHD\*"; DestDir: "{app}\templates\PTHD"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "..\..\m1-monitor\build\M1-Monitor_artefacts\Release\VST\M1-Monitor.dll"; DestDir: "{code:GetDir|0}"; Components: vst2; Flags: ignoreversion
Source: "..\..\m1-panner\build\M1-Panner_artefacts\Release\VST\M1-Panner.dll"; DestDir: "{code:GetDir|0}"; Components: vst2; Flags: ignoreversion
Source: "..\resources\templates\Reaper\*"; DestDir: "{app}\templates\Reaper"; Components: vst2; Flags: ignoreversion recursesubdirs createallsubdirs

//Source: "..\..\m1-monitor\build\M1-Monitor_artefacts\Release\VST3\M1-Monitor.vst3"; DestDir: "{code:GetDir|1}"; Components: vst3; Flags: ignoreversion
//Source: "..\..\m1-panner\build\M1-Panner_artefacts\Release\VST3\M1-Panner.vst3"; DestDir: "{code:GetDir|1}"; Components: vst3; Flags: ignoreversion

Source: "..\..\m1-transcoder\dist\M1-Transcoder.exe"; Excludes: "ffmpeg.exe,ffmpeg.dll"; DestDir: "{app}\M1-Transcoder"; Components: m1transcoder; Flags: ignoreversion
Source: "..\resources\docs\Mach1-Monitor.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\resources\docs\Mach1-Panner.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\resources\docs\Mach1-Transcoder.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\resources\docs\Mach1-UserGuide.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "..\..\m1-player\build\M1-Player_artefacts\Release\*"; Excludes: "ffmpeg*.zip,*.exp,*M1-Player.lib,*av*.dll,postproc*.dll,sw*.dll"; DestDir: "{app}\M1-Player"; Components: m1player; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "..\..\m1-orientationmanager\build\M1-orientationmanager_artefacts\Release\m1-orientationmanager.exe"; Excludes: "ffmpeg*.zip,*.exp,*m1-orientationmanager.lib,*av*.dll,postproc*.dll,sw*.dll"; DestDir: "{app}\services\m1-orientationmanager.exe"; Components: m1services; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\services\m1-system-helper\build\M1-system-helper_artefacts\Release\m1-system-helper.exe"; DestDir: "{app}\services\m1-system-helper.exe"; Components: m1services; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{src}\version_cleanup.bat"; Parameters: "install"; Flags: runhidden
Filename: "{src}\om_bootstrap.bat"; Parameters: "install"; Flags: runhidden

[Messages]
SetupWindowTitle=Install Mach1 Spatial System
WelcomeLabel1=Mach1 Spatial System

[Icons]
Name: "{group}\M1-Player"; Filename: "{app}\M1-Player.exe"; Components: m1player
Name: "{group}\M1-Transcoder"; Filename: "{app}\M1-Transcoder.exe"; Components: m1transcoder
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"

[Code]
var
Page1: TInputDirWizardPage;

procedure InitializeWizard;
begin
    WizardForm.WelcomeLabel2.AutoSize := True;

    Page1 := CreateInputDirPage(wpSelectComponents, 'VST/VST3 Plugin Folders',
    'Please select your VST/VST3 plugin directories',
    '',
    False, 'New Folder');
    Page1.Add('VST 64-bit plug-ins location');
    Page1.Values[0] := ExpandConstant('{pf64}\Steinberg\VstPlugins');
    //Page1.Add('VST3 64-bit plug-ins location');
    //Page1.Values[1] := ExpandConstant('{pf64}\Common Files\VST3');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    //SaveStringToFile(ExpandConstant('{userappdata}/Mach1 Spatial System/path.data'), ExpandConstant('{app}'), False);
  end;
end;

function GetDir(Param: String): String;
begin
  Result := Page1.Values[StrToInt(Param)];
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  case PageID of
    Page1.ID : Result := not IsComponentSelected('vst2');
  else
    Result := False;
  end;
end;
