#Requires -Version 5.1
<#
.SYNOPSIS
    MCSkin RaytraceRenderer - Windows release packaging script
.DESCRIPTION
    Builds Release, runs windeployqt, packages into a distributable zip.
#>

param(
    [string]$QtPath = "",
    [string]$Version = "1.0.0",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Write-Info  { param($Msg) Write-Host "[INFO] " -ForegroundColor Green -NoNewline; Write-Host $Msg }
function Write-Warn  { param($Msg) Write-Host "[WARN] " -ForegroundColor Yellow -NoNewline; Write-Host $Msg }
function Write-Err   { param($Msg) Write-Host "[ERROR] " -ForegroundColor Red -NoNewline; Write-Host $Msg }

$ProjectDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir   = Join-Path $ProjectDir "build"
$ReleaseDir = Join-Path $ProjectDir "release"
$AppName    = "MCSkin_RaytraceRenderer"
$ExeName    = "mcskin_raytracer.exe"

# -- Detect Qt path --

function Find-QtPath {
    # 1. Explicit parameter
    if ($QtPath -and (Test-Path $QtPath)) { return $QtPath }

    # 2. CMAKE_PREFIX_PATH env
    if ($env:CMAKE_PREFIX_PATH -and (Test-Path "$env:CMAKE_PREFIX_PATH\bin\windeployqt.exe")) {
        return $env:CMAKE_PREFIX_PATH
    }

    # 3. Auto-detect common locations (msvc only)
    $searchPaths = @("C:\Qt\6.*\msvc*", "$env:USERPROFILE\Qt\6.*\msvc*")
    foreach ($pattern in $searchPaths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($found -and (Test-Path "$($found.FullName)\bin\windeployqt.exe")) {
            return $found.FullName
        }
    }

    return $null
}

$QtRoot = Find-QtPath
if (-not $QtRoot) {
    Write-Err "Cannot find Qt6 MSVC installation."
    Write-Host "  Specify with: .\scripts\package.ps1 -QtPath 'C:\Qt\6.10.2\msvc2022_64'" -ForegroundColor Yellow
    exit 1
}

$WinDeployQt = Join-Path $QtRoot "bin\windeployqt.exe"
Write-Info "Qt path: $QtRoot"
Write-Info "windeployqt: $WinDeployQt"

# -- Build Release --

if (-not $SkipBuild) {
    Write-Host ""
    Write-Info "Building Release..."

    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    & cmake -S $ProjectDir -B $BuildDir -DCMAKE_PREFIX_PATH="$QtRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
    if ($LASTEXITCODE -ne 0) { Write-Err "CMake configure failed."; exit 1 }

    $jobs = if ($env:NUMBER_OF_PROCESSORS) { $env:NUMBER_OF_PROCESSORS } else { "4" }
    & cmake --build $BuildDir --config Release --target mcskin_raytracer -j $jobs
    if ($LASTEXITCODE -ne 0) { Write-Err "Build failed."; exit 1 }

    Write-Info "Build complete."
}

# -- Verify exe exists --

$ExePath = Join-Path $BuildDir "src\Release\$ExeName"
if (-not (Test-Path $ExePath)) {
    Write-Err "Executable not found: $ExePath"
    Write-Host "  Make sure the build succeeded, or run without -SkipBuild" -ForegroundColor Yellow
    exit 1
}

# -- Prepare release directory --

Write-Host ""
Write-Info "Preparing release directory..."

if (Test-Path $ReleaseDir) {
    Remove-Item -Recurse -Force $ReleaseDir
}
New-Item -ItemType Directory -Path $ReleaseDir | Out-Null

Copy-Item $ExePath $ReleaseDir

# -- Run windeployqt --

Write-Info "Running windeployqt..."
$deployTarget = Join-Path $ReleaseDir $ExeName
& $WinDeployQt $deployTarget --release --no-translations --no-system-d3d-compiler --no-opengl-sw
if ($LASTEXITCODE -ne 0) {
    Write-Warn "windeployqt returned non-zero, checking result..."
}

# -- Verify key DLLs --

$requiredDlls = @("Qt6Widgets.dll", "Qt6Core.dll", "Qt6Gui.dll", "Qt6OpenGL.dll", "Qt6OpenGLWidgets.dll", "Qt6Network.dll")
$allFound = $true
foreach ($dll in $requiredDlls) {
    if (-not (Test-Path (Join-Path $ReleaseDir $dll))) {
        Write-Err "Missing: $dll"
        $allFound = $false
    }
}
if (-not $allFound) {
    Write-Err "Some Qt DLLs are missing. Check windeployqt output."
    exit 1
}
Write-Info "All required Qt DLLs present."

# -- Copy MSVC runtime if available --

$vcRedistDlls = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll")
$sysDir = "$env:SystemRoot\System32"
$copiedVcrt = 0
foreach ($dll in $vcRedistDlls) {
    $src = Join-Path $sysDir $dll
    if (Test-Path $src) {
        Copy-Item $src $ReleaseDir
        $copiedVcrt++
    }
}
if ($copiedVcrt -gt 0) {
    Write-Info "Bundled $copiedVcrt MSVC runtime DLL(s)."
} else {
    Write-Warn "MSVC runtime DLLs not found in System32. Users may need VC++ Redistributable."
}

# -- Package zip --

Write-Host ""
$zipName = "${AppName}_v${Version}_win64.zip"
$zipPath = Join-Path $ProjectDir $zipName

if (Test-Path $zipPath) { Remove-Item $zipPath }

Write-Info "Packaging: $zipName"
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $zipPath

$sizeMB = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Host ""
Write-Info "Done. Package: $zipPath ($sizeMB MB)"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor White
Write-Host "  1) git tag v$Version" -ForegroundColor Cyan
Write-Host "  2) git push origin v$Version" -ForegroundColor Cyan
Write-Host "  3) Upload $zipName to GitHub Releases" -ForegroundColor Cyan
