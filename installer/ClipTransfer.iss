; Inno Setup script for ClipTransfer
; Requirements:
; - Inno Setup (ISCC.exe)
; - A prepared deployment folder containing ClipTransfer.exe + Qt runtime (see scripts/package-windows.ps1)

#define MyAppName "ClipTransfer"
#define MyAppExeName "ClipTransfer.exe"
#define MyAppPublisher "Evr5"
#define MyAppURL "https://github.com/Evr5/ClipTransfer"
#define MyAppVersion "3.0.0"

; Paths (robust, based on this .iss file location)
#define RepoRoot SourcePath + "..\\"

; This folder is produced by scripts/package-windows.ps1
#define DeployDir RepoRoot + "dist\\windows\\deploy"

#if !FileExists(RepoRoot + "res\\icon.ico")
  #error "Missing icon: " + (RepoRoot + "res\\icon.ico")
#endif

#if !FileExists(DeployDir + "\\" + MyAppExeName)
  #error "Missing deployment folder: " + DeployDir + " (generate it via scripts\\package-windows.ps1)"
#endif

[Setup]
AppId={{8A1C5C64-5077-48C3-9C1B-8F9D2A8B1C0A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; Auto-detect installer language from Windows UI language, do not ask user
ShowLanguageDialog=no
LanguageDetectionMethod=uiLanguage
; Allow user to change install location
DisableDirPage=no
OutputDir=..\\dist\\windows
OutputBaseFilename={#MyAppName}-Setup-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\\res\\icon.ico
UninstallDisplayIcon={app}\\{#MyAppExeName}
UninstallDisplayName={#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\\French.isl"

[CustomMessages]
french.NicknameTitle=Pseudo appareil
french.NicknameSubTitle=Choisissez le pseudo utilisé dans le programme pour différencier vos appareils
french.NicknameDescription=Ce pseudo sera utilisé par défaut dans les communications LAN par le programme pour indiquer qui a envoyé le message.
french.NicknameLabel=Pseudo :

english.NicknameTitle=User device
english.NicknameSubTitle=Choose the nickname used in the program to differentiate your devices
english.NicknameDescription=This nickname will be used by default in LAN communications by the program to indicate who sent the message.
english.NicknameLabel=Nickname:

french.AppLanguageTitle=Langue de l'application
french.AppLanguageSubTitle=Choisis la langue de ClipTransfer
french.AppLanguageFrench=Français
french.AppLanguageEnglish=Anglais
french.AppLanguageSpanish=Espagnol
french.AppLanguagePortuguese=Portugais
french.AppLanguageGerman=Allemand
french.AppLanguageDutch=Néerlandais
french.AppLanguageChinese=Chinois
french.AppLanguageArabic=Arabe
french.AppLanguagePolish=Polonais

english.AppLanguageTitle=Application language
english.AppLanguageSubTitle=Choose ClipTransfer language
english.AppLanguageFrench=French
english.AppLanguageEnglish=English
english.AppLanguageSpanish=Spanish
english.AppLanguagePortuguese=Portuguese
english.AppLanguageGerman=German
english.AppLanguageDutch=Dutch
english.AppLanguageChinese=Chinese
english.AppLanguageArabic=Arabic
english.AppLanguagePolish=Polish

french.ErrorNicknameRequired=Le pseudo est obligatoire.
french.ErrorNicknamePipe=Le caractère '|' n'est pas autorisé.

english.ErrorNicknameRequired=Nickname is required.
english.ErrorNicknamePipe=The '|' character is not allowed.

[Files]
; Everything that windeployqt produced + i18n folder
Source: "{#DeployDir}\\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\\{#MyAppName}"; Filename: "{app}\\{#MyAppExeName}"
Name: "{group}\\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\\{#MyAppName}"; Filename: "{app}\\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
; Qt QSettings on Windows defaults to HKCU\Software\<OrganizationName>\<ApplicationName>
; In code: OrganizationName = "ClipTransfer" and ApplicationName = "ClipTransfer"
; Keys used:
; - settings.value("ui/language") -> group "ui", value "language"
; - settings.value("user/nickname") -> group "user", value "nickname"
Root: HKCU; Subkey: "Software\\ClipTransfer\\ClipTransfer\\ui"; ValueType: string; ValueName: "language"; ValueData: "{code:GetAppLanguage}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\\ClipTransfer\\ClipTransfer\\user"; ValueType: string; ValueName: "nickname"; ValueData: "{code:GetNickname}"; Flags: uninsdeletekeyifempty
; Ensure the whole app settings tree is removed on uninstall
Root: HKCU; Subkey: "Software\\ClipTransfer\\ClipTransfer"; Flags: uninsdeletekey

[UninstallDelete]
; Remove the whole installation directory (even if it contains extra files)
Type: filesandordirs; Name: "{app}"

[Code]
var
  NicknamePage: TInputQueryWizardPage;
  LangPage: TWizardPage;
  RbFrench: TRadioButton;
  RbEnglish: TRadioButton;
  RbSpanish: TRadioButton;
  RbPortuguese: TRadioButton;
  RbGerman: TRadioButton;
  RbDutch: TRadioButton;
  RbChinese: TRadioButton;
  RbArabic: TRadioButton;
  RbPolish: TRadioButton;

procedure InitializeWizard;
begin
  { App language (separate from installer UI language) }
  LangPage := CreateCustomPage(wpWelcome,
    ExpandConstant('{cm:AppLanguageTitle}'),
    ExpandConstant('{cm:AppLanguageSubTitle}'));

  RbFrench := TRadioButton.Create(LangPage);
  RbFrench.Parent := LangPage.Surface;
  RbFrench.Left := ScaleX(0);
  RbFrench.Top := ScaleY(8);
  RbFrench.Width := ScaleX(350);
  RbFrench.Caption := ExpandConstant('{cm:AppLanguageFrench}');

  RbEnglish := TRadioButton.Create(LangPage);
  RbEnglish.Parent := LangPage.Surface;
  RbEnglish.Left := ScaleX(0);
  RbEnglish.Top := RbFrench.Top + ScaleY(24);
  RbEnglish.Width := ScaleX(350);
  RbEnglish.Caption := ExpandConstant('{cm:AppLanguageEnglish}');

  RbSpanish := TRadioButton.Create(LangPage);
  RbSpanish.Parent := LangPage.Surface;
  RbSpanish.Left := ScaleX(0);
  RbSpanish.Top := RbEnglish.Top + ScaleY(24);
  RbSpanish.Width := ScaleX(350);
  RbSpanish.Caption := ExpandConstant('{cm:AppLanguageSpanish}');

  RbPortuguese := TRadioButton.Create(LangPage);
  RbPortuguese.Parent := LangPage.Surface;
  RbPortuguese.Left := ScaleX(0);
  RbPortuguese.Top := RbSpanish.Top + ScaleY(24);
  RbPortuguese.Width := ScaleX(350);
  RbPortuguese.Caption := ExpandConstant('{cm:AppLanguagePortuguese}');

  RbGerman := TRadioButton.Create(LangPage);
  RbGerman.Parent := LangPage.Surface;
  RbGerman.Left := ScaleX(0);
  RbGerman.Top := RbPortuguese.Top + ScaleY(24);
  RbGerman.Width := ScaleX(350);
  RbGerman.Caption := ExpandConstant('{cm:AppLanguageGerman}');

  RbDutch := TRadioButton.Create(LangPage);
  RbDutch.Parent := LangPage.Surface;
  RbDutch.Left := ScaleX(0);
  RbDutch.Top := RbGerman.Top + ScaleY(24);
  RbDutch.Width := ScaleX(350);
  RbDutch.Caption := ExpandConstant('{cm:AppLanguageDutch}');

  RbChinese := TRadioButton.Create(LangPage);
  RbChinese.Parent := LangPage.Surface;
  RbChinese.Left := ScaleX(0);
  RbChinese.Top := RbDutch.Top + ScaleY(24);
  RbChinese.Width := ScaleX(350);
  RbChinese.Caption := ExpandConstant('{cm:AppLanguageChinese}');

  RbArabic := TRadioButton.Create(LangPage);
  RbArabic.Parent := LangPage.Surface;
  RbArabic.Left := ScaleX(0);
  RbArabic.Top := RbChinese.Top + ScaleY(24);
  RbArabic.Width := ScaleX(350);
  RbArabic.Caption := ExpandConstant('{cm:AppLanguageArabic}');

  RbPolish := TRadioButton.Create(LangPage);
  RbPolish.Parent := LangPage.Surface;
  RbPolish.Left := ScaleX(0);
  RbPolish.Top := RbArabic.Top + ScaleY(24);
  RbPolish.Width := ScaleX(350);
  RbPolish.Caption := ExpandConstant('{cm:AppLanguagePolish}');

  { Default: follow the installer UI language }
  if ActiveLanguage = 'french' then
    RbFrench.Checked := True
  else
    RbEnglish.Checked := True;

  { Nickname }
  NicknamePage := CreateInputQueryPage(
    wpSelectDir,
    ExpandConstant('{cm:NicknameTitle}'),
    ExpandConstant('{cm:NicknameSubTitle}'),
    ExpandConstant('{cm:NicknameDescription}'));
  NicknamePage.Add(ExpandConstant('{cm:NicknameLabel}'), False);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Nick: String;
begin
  Result := True;

  if CurPageID = NicknamePage.ID then
  begin
    Nick := Trim(NicknamePage.Values[0]);
    if Nick = '' then
    begin
      MsgBox(ExpandConstant('{cm:ErrorNicknameRequired}'), mbError, MB_OK);
      Result := False;
      exit;
    end;

    if Pos('|', Nick) > 0 then
    begin
      MsgBox(ExpandConstant('{cm:ErrorNicknamePipe}'), mbError, MB_OK);
      Result := False;
      exit;
    end;
  end;
end;

function GetNickname(Param: String): String;
begin
  Result := Trim(NicknamePage.Values[0]);
end;

function GetAppLanguage(Param: String): String;
begin
  if RbFrench.Checked then
    Result := 'fr'
  else if RbEnglish.Checked then
    Result := 'en'
  else if RbSpanish.Checked then
    Result := 'es'
  else if RbPortuguese.Checked then
    Result := 'pt'
  else if RbGerman.Checked then
    Result := 'de'
  else if RbDutch.Checked then
    Result := 'nl'
  else if RbChinese.Checked then
    Result := 'zh'
  else if RbArabic.Checked then
    Result := 'ar'
  else if RbPolish.Checked then
    Result := 'pl'
  else
    Result := 'en';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  { No-op }
end;
