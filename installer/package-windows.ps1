Param(
  [ValidateSet('Release','Debug')]
  [string]$Config = 'Release',
  [string]$BuildDir = "${PSScriptRoot}\..\build",
  [string]$OutDir = "${PSScriptRoot}\..\dist\windows\deploy",
  [string]$QtBinDir = $env:QT_BIN_DIR
)

$ErrorActionPreference = 'Stop'

function Assert-Path([string]$Path, [string]$Hint) {
  if (-not (Test-Path $Path)) {
    throw "Path not found: $Path. $Hint"
  }
}

# Build
Push-Location (Resolve-Path "${PSScriptRoot}\..")
try {
  cmake --build $BuildDir --config $Config

  # Find exe (this repo currently links to project-root ClipTransfer.exe)
  $exeCandidates = @(
    (Join-Path (Get-Location) 'ClipTransfer.exe'),
    (Join-Path $BuildDir 'ClipTransfer.exe'),
    (Join-Path (Join-Path $BuildDir $Config) 'ClipTransfer.exe')
  )

  $exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $exePath) {
    throw "Unable to find ClipTransfer.exe (searched: $($exeCandidates -join ', '))"
  }

  # Prepare output
  New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
  Copy-Item -Force $exePath (Join-Path $OutDir 'ClipTransfer.exe')

  # Copy translations (.qm) into deploy/i18n
  $deployI18nDir = (Join-Path $OutDir 'i18n')
  New-Item -ItemType Directory -Force -Path $deployI18nDir | Out-Null

  $qmCandidates = @(
    (Join-Path $BuildDir '*.qm'),
    (Join-Path (Join-Path (Get-Location) 'i18n') '*.qm'),
    (Join-Path (Join-Path $BuildDir 'i18n') '*.qm')
  )

  $qmFiles = @()
  foreach ($pattern in $qmCandidates) {
    $qmFiles += Get-ChildItem -ErrorAction SilentlyContinue -File $pattern
  }
  $qmFiles = $qmFiles | Sort-Object -Property FullName -Unique

  if ($qmFiles.Count -gt 0) {
    foreach ($f in $qmFiles) {
      Copy-Item -Force $f.FullName (Join-Path $deployI18nDir $f.Name)
    }
  } else {
    Write-Host "Warning: no .qm translations found (expected build/*.qm or i18n/*.qm)."
  }

  # Deploy Qt runtime
  if (-not $QtBinDir) {
    # MSYS2 MinGW64 default
    $msys2Mingw64 = 'C:\msys64\mingw64\bin'
    if (Test-Path (Join-Path $msys2Mingw64 'windeployqt-qt6.exe')) {
      $QtBinDir = $msys2Mingw64
      Write-Host "QT_BIN_DIR not set: MSYS2 detected ($QtBinDir)"
    } else {
      Write-Host "QT_BIN_DIR not set: trying PATH (windeployqt-qt6.exe / windeployqt.exe)"
    }
  }

  if (-not $QtBinDir) {
    $windeployqtCmd = (Get-Command windeployqt-qt6.exe -ErrorAction SilentlyContinue)
    if (-not $windeployqtCmd) {
      $windeployqtCmd = (Get-Command windeployqt.exe -ErrorAction SilentlyContinue)
    }
    if ($windeployqtCmd) { $windeployqt = $windeployqtCmd.Source }
  } else {
    $candidates = @(
      (Join-Path $QtBinDir 'windeployqt-qt6.exe'),
      (Join-Path $QtBinDir 'windeployqt.exe')
    )
    $windeployqt = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  }

  if (-not $windeployqt) {
    throw "windeployqt not found. For MSYS2 MINGW64, set QT_BIN_DIR=C:\\msys64\\mingw64\\bin or add windeployqt-qt6.exe to PATH."
  }
  Assert-Path $windeployqt "Install Qt and ensure windeployqt is available."

  # --compiler-runtime is crucial for MSYS2/MinGW builds:
  # it copies libstdc++/libgcc/libwinpthread next to the exe.
  $deployArgs = @(
    '--no-translations',
    '--no-system-d3d-compiler',
    '--compiler-runtime'
  )
  if ($Config -eq 'Debug') { $deployArgs += '--debug' } else { $deployArgs += '--release' }
  $deployArgs += (Join-Path $OutDir 'ClipTransfer.exe')
  & $windeployqt @deployArgs

  Write-Host "OK: bundle ready in $OutDir"
  Write-Host "Next, to build the installer: ISCC.exe installer\ClipTransfer.iss"
}
finally {
  Pop-Location
}
