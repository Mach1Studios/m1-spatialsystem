[Setup]
AppName=Mach1 Spatial System
AppVersion=2.0.0
AppPublisher=Mach1
DefaultDirName={pf64}\Mach1
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
PrivilegesRequired=admin

[Types]
Name: full; Description: Full installation
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: aax; Description: AAX; Types: full
Name: vst2; Description: VST2; Types: full
//Name: vst3; Description: VST3
Name: m1player; Description: M1-Player; Types: full
Name: m1transcoder; Description: M1-Transcoder; Types: full custom; Flags: fixed
Name: m1services; Description: M1 Background Services; Types: full custom; Flags: fixed

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

Source: "..\..\m1-transcoder\dist\M1-Transcoder.exe"; Excludes: "ffmpeg.exe,ffmpeg.dll"; DestDir: "{app}"; Components: m1transcoder; Flags: ignoreversion; AfterInstall: RunPostinstallTranscoder
Source: "..\resources\docs\Mach1-Monitor.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\resources\docs\Mach1-Panner.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\resources\docs\Mach1-Transcoder.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\resources\docs\Mach1-UserGuide.pdf"; DestDir: "{app}\docs"; Components: m1transcoder; Flags: ignoreversion recursesubdirs createallsubdirs

Source: "preinstall_player.bat"; Flags: dontcopy noencryption
Source: "postinstall_transcoder.bat"; Flags: dontcopy noencryption
Source: "download_ffmpeg.bat"; DestDir: "{app}"; Components: m1player; Flags: noencryption ignoreversion
Source: "..\..\m1-player\build\M1-Player_artefacts\Release\M1-Player.exe"; Excludes: "ffmpeg*.zip,*.exp,*M1-Player.lib"; DestDir: "{app}"; Components: m1player; Flags: ignoreversion recursesubdirs createallsubdirs; BeforeInstall: RunPreinstallPlayer

Source: "service_setup.bat"; Flags: dontcopy noencryption
Source: "service_stopper.bat"; Flags: dontcopy noencryption
Source: "..\..\m1-orientationmanager\build\M1-orientationmanager_artefacts\Release\m1-orientationmanager.exe"; Excludes: "ffmpeg*.zip,*.exp,*m1-orientationmanager.lib,*av*.dll,postproc*.dll,sw*.dll"; DestDir: "{commonappdata}\Mach1"; Components: m1services; Flags: ignoreversion recursesubdirs createallsubdirs; BeforeInstall: StopServices
Source: "..\..\services\m1-system-helper\build\M1-system-helper_artefacts\Release\m1-system-helper.exe"; DestDir: "{commonappdata}\Mach1"; Components: m1services; Flags: ignoreversion recursesubdirs createallsubdirs; BeforeInstall: StopServices; AfterInstall: SetupServices
Source: "..\..\m1-orientationmanager\Resources\settings.json"; DestDir: "{commonappdata}\Mach1"; Components: m1services; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{app}\download_ffmpeg.bat"; Parameters: "install"; Flags: postinstall runascurrentuser; StatusMsg: "Downloading required ffmpeg libs..."

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

procedure RunPreinstallPlayer();
var
  ResultCode: integer;
begin
  // Launch bat script
  ExtractTemporaryFile('preinstall_player.bat');
  if Exec(ExpandConstant('{tmp}\preinstall_player.bat'), '', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    // handle success
  end
  else begin
    // handle failure
  end;
end;

procedure SetupServices();
var
  ResultCode: integer;
begin
  // Launch bat script
  ExtractTemporaryFile('service_setup.bat');
  if Exec(ExpandConstant('{tmp}\service_setup.bat'), '', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    // handle success
  end
  else begin
    // handle failure
  end;
end;

procedure StopServices();
var
  ResultCode: integer;
begin
  // Launch bat script
  ExtractTemporaryFile('service_stopper.bat');
  if Exec(ExpandConstant('{tmp}\service_stopper.bat'), '', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    // handle success
  end
  else begin
    // handle failure
  end;
end;

procedure RunPostinstallTranscoder();
var
  ResultCode: integer;
begin
  // Launch bat script
  ExtractTemporaryFile('postinstall_transcoder.bat');
  if Exec(ExpandConstant('{tmp}\postinstall_transcoder.bat'), '', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    // handle success
  end
  else begin
    // handle failure
  end;
end;