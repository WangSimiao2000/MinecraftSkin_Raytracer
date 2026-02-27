#Requires -Version 5.1
<#
.SYNOPSIS
    MCSkin RaytraceRenderer - Windows build setup script
.DESCRIPTION
    Checks dependencies, offers installation, selects build modules.
#>

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# -- Output helpers --

function Write-Info  { param($Msg) Write-Host "[INFO] " -ForegroundColor Green -NoNewline; Write-Host $Msg }
function Write-Warn  { param($Msg) Write-Host "[WARN] " -ForegroundColor Yellow -NoNewline; Write-Host $Msg }
function Write-Err   { param($Msg) Write-Host "[ERROR] " -ForegroundColor Red -NoNewline; Write-Host $Msg }
function Write-Ask   { param($Msg) Write-Host "[?] " -ForegroundColor Cyan -NoNewline; Write-Host $Msg -NoNewline }

$ProjectDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

# -- Dependency checks --

function Test-Cmd { param($Name) $null -ne (Get-Command $Name -ErrorAction SilentlyContinue) }

function Test-CMake {
    if (-not (Test-Cmd "cmake")) { return $false }
    $verLine = cmake --version | Select-Object -First 1
    if ($verLine -match '(\d+)\.(\d+)') {
        $major = [int]$Matches[1]; $minor = [int]$Matches[2]
        return ($major -gt 3 -or ($major -eq 3 -and $minor -ge 22))
    }
    return $false
}

function Test-Compiler {
    (Test-Cmd "cl") -or (Test-Cmd "clang++") -or (Test-Cmd "g++")
}

function Test-Git { Test-Cmd "git" }

function Test-Qt6 {
    if (Test-Cmd "qmake6") { return $true }
    if (Test-Cmd "qmake") {
        $out = & qmake --version 2>&1 | Out-String
        if ($out -match "Qt version 6") { return $true }
    }
    $patterns = @(
        "C:\Qt\6.*\msvc*\bin",
        "$env:USERPROFILE\Qt\6.*\msvc*\bin"
    )
    foreach ($p in $patterns) {
        if (Get-Item $p -ErrorAction SilentlyContinue) { return $true }
    }
    return $false
}

function Test-OpenGL { return $true }

# -- Dependency table --

$DepNames  = @("cmake",          "compiler",        "git",  "qt6",                        "opengl")
$DepLabels = @("CMake >= 3.22",  "C++ Compiler",    "Git",  "Qt6 Widgets/OpenGL/Network", "OpenGL")
$DepChecks = @(
    { Test-CMake },
    { Test-Compiler },
    { Test-Git },
    { Test-Qt6 },
    { Test-OpenGL }
)

$script:missingManual = @()

function Check-AllDeps {
    Write-Info "Checking dependencies..."
    Write-Host ""

    for ($i = 0; $i -lt $DepNames.Count; $i++) {
        $ok = & $DepChecks[$i]
        $label = $DepLabels[$i]
        if ($ok) {
            Write-Host "  " -NoNewline
            Write-Host "[OK]" -ForegroundColor Green -NoNewline
            Write-Host " $label"
        }
        else {
            Write-Host "  " -NoNewline
            Write-Host "[X]" -ForegroundColor Red -NoNewline
            Write-Host " $label  " -NoNewline
            Write-Host "(manual install required)" -ForegroundColor Red
            $script:missingManual += @{ Index = $i; Name = $DepNames[$i]; Label = $label }
        }
    }
    Write-Host ""
}

function Show-ManualHint {
    param([string]$DepName)
    switch ($DepName) {
        "cmake"    { Write-Host "    -> https://cmake.org/download/" -ForegroundColor Yellow }
        "compiler" {
            Write-Host "    -> Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
            Write-Host "    -> LLVM/Clang: https://releases.llvm.org/" -ForegroundColor Yellow
        }
        "git"      { Write-Host "    -> https://git-scm.com/download/win" -ForegroundColor Yellow }
        "qt6"      {
            Write-Host "    -> Qt Online Installer: https://www.qt.io/download-qt-installer" -ForegroundColor Yellow
            Write-Host '    -> cmake -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64" ..' -ForegroundColor Yellow
        }
    }
}

function Handle-MissingDeps {
    if ($script:missingManual.Count -gt 0) {
        Write-Warn "The following must be installed manually:"
        foreach ($dep in $script:missingManual) {
            Write-Host "  * $($dep.Label)" -ForegroundColor Red
            Show-ManualHint $dep.Name
        }
        Write-Host ""
        Write-Ask "Continue anyway? Build may fail. [y/N] "
        $answer = Read-Host
        if ($answer -notmatch "^[Yy]") {
            Write-Err "Please install missing dependencies first."
            exit 1
        }
    }
    else {
        Write-Info "All dependencies ready."
        Write-Host ""
    }
}

# -- Module selection --

$script:BuildApp   = $true
$script:BuildTests = $true
$script:BuildType  = "Release"
$script:CMakeExtra = @()

function Select-Modules {
    Write-Host ""
    Write-Host "Select build modules:" -ForegroundColor White
    Write-Host "  1) All  -  app + tests"
    Write-Host "  2) App only  -  mcskin_raytracer"
    Write-Host "  3) Tests only  -  mcskin_tests"
    Write-Ask "Choice [1/2/3], default 1: "
    $choice = Read-Host
    switch ($choice) {
        "2" {
            $script:BuildApp = $true
            $script:BuildTests = $false
            Write-Info "Build: app only"
        }
        "3" {
            $script:BuildApp = $false
            $script:BuildTests = $true
            Write-Info "Build: tests only"
        }
        default {
            $script:BuildApp = $true
            $script:BuildTests = $true
            Write-Info "Build: all"
        }
    }

    Write-Host ""
    Write-Host "Build type:" -ForegroundColor White
    Write-Host "  1) Release   2) Debug   3) RelWithDebInfo"
    Write-Ask "Choice [1/2/3], default 1: "
    $bt = Read-Host
    switch ($bt) {
        "2" { $script:BuildType = "Debug" }
        "3" { $script:BuildType = "RelWithDebInfo" }
        default { $script:BuildType = "Release" }
    }
    Write-Info "Build type: $($script:BuildType)"
    Write-Host ""

    # Qt6 path
    if (-not $env:CMAKE_PREFIX_PATH) {
        Write-Host 'Enter Qt6 path for CMAKE_PREFIX_PATH, leave empty to skip.' -ForegroundColor Yellow
        Write-Host '  Example: C:\Qt\6.8.0\msvc2022_64' -ForegroundColor Yellow
        Write-Ask "Qt6 path: "
        $qtPath = Read-Host
        if ($qtPath) {
            $script:CMakeExtra += "-DCMAKE_PREFIX_PATH=$qtPath"
        }
    }
}

# -- Build --

function Build-Project {
    Write-Info "Starting build..."

    $buildDir = Join-Path $ProjectDir "build"
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
    }

    $cmakeArgs = @(
        "-S", $ProjectDir,
        "-B", $buildDir,
        "-DCMAKE_BUILD_TYPE=$($script:BuildType)"
    )

    if ($script:BuildTests) {
        $cmakeArgs += "-DBUILD_TESTS=ON"
    }
    else {
        $cmakeArgs += "-DBUILD_TESTS=OFF"
    }

    $cmakeArgs += $script:CMakeExtra

    Write-Info "cmake $($cmakeArgs -join ' ')"
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Err "CMake configure failed."
        exit 1
    }

    $jobs = $env:NUMBER_OF_PROCESSORS
    if (-not $jobs) { $jobs = "4" }

    $targets = @()
    if ($script:BuildApp)   { $targets += "mcskin_raytracer" }
    if ($script:BuildTests) { $targets += "mcskin_tests" }

    foreach ($t in $targets) {
        Write-Info "Building: $t ($($script:BuildType))"
        & cmake --build $buildDir --target $t --config $script:BuildType -j $jobs
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Build failed: $t"
            exit 1
        }
    }

    Write-Host ""
    Write-Info "Build complete."
    if ($script:BuildApp) {
        Write-Info "Executable: $buildDir\src\$($script:BuildType)\mcskin_raytracer.exe"
    }
    if ($script:BuildTests) {
        Write-Info "Tests: $buildDir\tests\$($script:BuildType)\mcskin_tests.exe"
        Write-Host ""
        Write-Ask "Run tests now? [y/N] "
        $rt = Read-Host
        if ($rt -match "^[Yy]") {
            Write-Info "Running tests..."
            Push-Location $buildDir
            & ctest --build-config $script:BuildType --output-on-failure
            Pop-Location
        }
    }
}

# -- Main --

Write-Host ""
Write-Host "==================================================" -ForegroundColor White
Write-Host "  MCSkin RaytraceRenderer - Windows Build Setup" -ForegroundColor White
Write-Host "==================================================" -ForegroundColor White
Write-Host ""

Check-AllDeps
Handle-MissingDeps
Select-Modules

Write-Ask "Start build? [Y/n] "
$doBuild = Read-Host
if ($doBuild -eq "" -or $doBuild -match "^[Yy]") {
    Build-Project
}
else {
    Write-Info "Skipped. Manual build:"
    Write-Host "  mkdir build; cd build"
    Write-Host "  cmake .."
    Write-Host "  cmake --build . --config $($script:BuildType) -j $env:NUMBER_OF_PROCESSORS"
}

Write-Host ""
Write-Info "GLM, Google Test, RapidCheck will be fetched automatically on first build via FetchContent."
