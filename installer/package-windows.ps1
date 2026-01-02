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

function Get-UniqueExistingDirs([string[]]$Dirs) {
  return $Dirs |
    Where-Object { $_ -and (Test-Path $_) } |
    Select-Object -Unique
}

function Is-SystemDllName([string]$Name) {
  if (-not $Name) { return $true }
  $n = $Name.ToLowerInvariant()

  if ($n.StartsWith('api-ms-win-') -or $n.StartsWith('ext-ms-win-')) { return $true }

  $sys32 = Join-Path $env:WINDIR "System32\$Name"
  $wow64 = Join-Path $env:WINDIR "SysWOW64\$Name"
  if (Test-Path $sys32 -or Test-Path $wow64) { return $true }

  return $false
}

function Find-DllInDirs([string]$DllName, [string[]]$SearchDirs) {
  foreach ($dir in $SearchDirs) {
    $candidate = Join-Path $dir $DllName
    if (Test-Path $candidate) { return $candidate }
  }
  return $null
}

function Copy-DllIfMissing([string]$DllName, [string[]]$SearchDirs, [string]$DestDir) {
  $destPath = Join-Path $DestDir $DllName
  if (Test-Path $destPath) { return $true }

  $found = Find-DllInDirs -DllName $DllName -SearchDirs $SearchDirs
  if ($found) {
    Copy-Item -Force $found $destPath
    Write-Host "Added runtime: $DllName"
    return $true
  }

  Write-Host "Warning: runtime dll not found: $DllName (searched: $($SearchDirs -join ', '))"
  return $false
}

function Copy-DllPatternIfMissing([string]$Pattern, [string[]]$SearchDirs, [string]$DestDir) {
  $copiedAny = $false

  foreach ($dir in $SearchDirs) {
    $matches = Get-ChildItem -Path $dir -Filter $Pattern -File -ErrorAction SilentlyContinue
    foreach ($m in $matches) {
      $destPath = Join-Path $DestDir $m.Name
      if (-not (Test-Path $destPath)) {
        Copy-Item -Force $m.FullName $destPath
        Write-Host "Added runtime: $($m.Name)"
        $copiedAny = $true
      }
    }
  }

  if (-not $copiedAny) {
    Write-Host "Warning: no dll matched pattern: $Pattern (searched: $($SearchDirs -join ', '))"
  }

  return $copiedAny
}

function Find-Objdump([string[]]$SearchDirs) {
  $cmd = Get-Command objdump.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }

  foreach ($dir in $SearchDirs) {
    $c = Join-Path $dir 'objdump.exe'
    if (Test-Path $c) { return $c }
  }

  return $null
}

function Get-PeDependencies([string]$ObjdumpPath, [string]$FilePath) {
  # Returns array of DLL names (strings)
  if (-not $ObjdumpPath) { return @() }
  if (-not (Test-Path $FilePath)) { return @() }

  $lines = & $ObjdumpPath -p $FilePath 2>$null
  if (-not $lines) { return @() }

  $deps = @()
  foreach ($line in $lines) {
    # Example: "DLL Name: KERNEL32.dll"
    if ($line -match 'DLL Name:\s*(.+)$') {
      $name = $Matches[1].Trim()
      if ($name) { $deps += $name }
    }
  }

  return $deps | Sort-Object -Unique
}

function Resolve-And-Copy-Dependencies([string]$ObjdumpPath, [string]$DestDir, [string[]]$SearchDirs) {
  # BFS over all exe/dll in DestDir (including plugins subfolders)
  $queue = New-Object System.Collections.Generic.Queue[string]
  $seen = New-Object 'System.Collections.Generic.HashSet[string]'

  $roots = Get-ChildItem -Path $DestDir -Recurse -File -Include *.exe,*.dll -ErrorAction SilentlyContinue
  foreach ($r in $roots) {
    $queue.Enqueue($r.FullName) | Out-Null
  }

  while ($queue.Count -gt 0) {
    $file = $queue.Dequeue()

    if ($seen.Contains($file)) { continue }
    $seen.Add($file) | Out-Null

    $deps = Get-PeDependencies -ObjdumpPath $ObjdumpPath -FilePath $file
    foreach ($dep in $deps) {
      if (Is-SystemDllName $dep) { continue }

      $destPath = Join-Path $DestDir $dep
      if (-not (Test-Path $destPath)) {
        $src = Find-DllInDirs -DllName $dep -SearchDirs $SearchDirs
        if ($src) {
          Copy-Item -Force $src $destPath
          Write-Host "Added dependency: $dep"

          # Newly copied DLL may itself have deps
          $queue.Enqueue($destPath) | Out-Null
        } else {
          Write-Host "Warning: missing dependency not found in search dirs: $dep"
        }
      }
    }
  }
}

# Build
Push-Location (Resolve-Path "${PSScriptRoot}\..")
try {
  # Build (optional: only if BuildDir exists)
  if (Test-Path $BuildDir) {
    cmake --build $BuildDir --config $Config
    if ($LASTEXITCODE -ne 0) {
      throw "cmake build failed with exit code $LASTEXITCODE (BuildDir=$BuildDir)"
    }
  } else {
    Write-Host "Warning: BuildDir not found ($BuildDir) -> skipping build step (will package existing exe if found)."
  }

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
    throw "windeployqt not found. For MSYS2, set QT_BIN_DIR to your Qt bin dir (e.g. C:\msys64\mingw64\bin)."
  }
  Assert-Path $windeployqt "Install Qt and ensure windeployqt is available."

  # --compiler-runtime helps but is not always sufficient on MSYS2 setups
  $deployArgs = @(
    '--no-translations',
    '--no-system-d3d-compiler',
    '--compiler-runtime'
  )
  if ($Config -eq 'Debug') { $deployArgs += '--debug' } else { $deployArgs += '--release' }
  $deployArgs += (Join-Path $OutDir 'ClipTransfer.exe')
  & $windeployqt @deployArgs
  if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
  }

  # Build a robust list of dirs to search for non-Qt DLL dependencies
  $runtimeSearchDirs = @()

  if ($QtBinDir) { $runtimeSearchDirs += $QtBinDir }

  $runtimeSearchDirs += @(
    'C:\msys64\mingw64\bin',
    'C:\msys64\ucrt64\bin',
    'C:\msys64\clang64\bin'
  )

  $gpp = Get-Command g++.exe -ErrorAction SilentlyContinue
  if ($gpp) { $runtimeSearchDirs += (Split-Path $gpp.Source) }

  $runtimeSearchDirs = Get-UniqueExistingDirs $runtimeSearchDirs

  # 1) Known compiler runtime DLLs (your first errors)
  $compilerRuntimeDlls = @(
    'libstdc++-6.dll',
    'libgcc_s_seh-1.dll',
    'libwinpthread-1.dll'
  )
  foreach ($dll in $compilerRuntimeDlls) {
    Copy-DllIfMissing -DllName $dll -SearchDirs $runtimeSearchDirs -DestDir $OutDir | Out-Null
  }

  # 2) A few common MSYS2 deps we already saw in your reports
  $commonDeps = @(
    'libb2-1.dll',
    'libdouble-conversion.dll',
    'libpcre2-16-0.dll',
    'zlib1.dll',
    'libzstd.dll'
  )
  foreach ($dll in $commonDeps) {
    Copy-DllIfMissing -DllName $dll -SearchDirs $runtimeSearchDirs -DestDir $OutDir | Out-Null
  }

  # ICU (versioned; copy whatever matches in your toolchain)
  Copy-DllPatternIfMissing -Pattern 'libicuin*.dll' -SearchDirs $runtimeSearchDirs -DestDir $OutDir | Out-Null
  Copy-DllPatternIfMissing -Pattern 'libicuuc*.dll' -SearchDirs $runtimeSearchDirs -DestDir $OutDir | Out-Null
  Copy-DllPatternIfMissing -Pattern 'libicudt*.dll' -SearchDirs $runtimeSearchDirs -DestDir $OutDir | Out-Null

  # 3) Automatic dependency resolve: scan exe/dlls in OutDir and pull missing dlls from MSYS2/Qt bin dirs
  $objdump = Find-Objdump -SearchDirs $runtimeSearchDirs
  if (-not $objdump) {
    Write-Host "Warning: objdump.exe not found; automatic dependency resolution disabled."
    Write-Host "         Install MSYS2 toolchain (mingw64) or ensure objdump.exe is in PATH."
  } else {
    Resolve-And-Copy-Dependencies -ObjdumpPath $objdump -DestDir $OutDir -SearchDirs $runtimeSearchDirs
  }

  Write-Host "OK: bundle ready in $OutDir"
  Write-Host "Next, to build the installer: ISCC.exe installer\ClipTransfer.iss"
}
finally {
  Pop-Location
}