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
$script:Generator  = $null

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
            
            # If using MinGW Qt, add MinGW bin to PATH
            if ($qtPath -match "mingw") {
                $mingwBin = Join-Path $qtPath "bin"
                if (Test-Path $mingwBin) {
                    $env:Path = "$mingwBin;$env:Path"
                    Write-Info "Added Qt MinGW bin to PATH: $mingwBin"
                }
                # Also check for MinGW tools in Qt installation
                $qtRoot = Split-Path (Split-Path $qtPath)
                $mingwTools = Get-ChildItem "$qtRoot\Tools\mingw*\bin" -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($mingwTools) {
                    $env:Path = "$($mingwTools.FullName);$env:Path"
                    Write-Info "Added MinGW tools to PATH: $($mingwTools.FullName)"
                }
            }
        }
    }
}

# -- Build --

function Build-Project {
    Write-Info "Starting build..."

    $buildDir = Join-Path $ProjectDir "build"
    
    # Check for generator mismatch and clean if needed
    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (Test-Path $cacheFile) {
        $cacheContent = Get-Content $cacheFile -Raw
        if ($cacheContent -match "CMAKE_GENERATOR:INTERNAL=(.+)") {
            $cachedGenerator = $Matches[1]
            # Determine current generator first
            $currentGenerator = $null
            if (Test-Cmd "ninja") {
                $currentGenerator = "Ninja"
            }
            elseif (Test-Cmd "mingw32-make") {
                $currentGenerator = "MinGW Makefiles"
            }
            elseif (Test-Cmd "g++") {
                $currentGenerator = "MinGW Makefiles"
            }
            elseif (Test-Cmd "cl") {
                $currentGenerator = "Visual Studio 17 2022"
            }
            
            if ($currentGenerator -and $cachedGenerator -ne $currentGenerator) {
                Write-Warn "Generator mismatch detected: cached=$cachedGenerator, current=$currentGenerator"
                Write-Warn "Cleaning build directory..."
                Remove-Item -Recurse -Force $buildDir
                New-Item -ItemType Directory -Path $buildDir | Out-Null
            }
        }
    }
    
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
    }

    # Auto-detect generator based on available tools
    $generator = $null
    
    if (Test-Cmd "ninja") {
        $generator = "Ninja"
        Write-Info "Using Ninja generator"
    }
    elseif (Test-Cmd "mingw32-make") {
        $generator = "MinGW Makefiles"
        Write-Info "Using MinGW Makefiles generator"
    }
    elseif (Test-Cmd "g++") {
        $generator = "MinGW Makefiles"
        Write-Info "Using MinGW Makefiles generator (g++ detected)"
    }
    elseif (Test-Cmd "cl") {
        $generator = "Visual Studio 17 2022"
        Write-Info "Using Visual Studio 2022 generator"
    }
    else {
        Write-Err "No suitable build tool found. Please install Ninja, MinGW, or Visual Studio."
        exit 1
    }
    
    $script:Generator = $generator
    
    $cmakeArgs = @(
        "-S", $ProjectDir,
        "-B", $buildDir,
        "-G", $generator
    )
    
    if ($generator -eq "Visual Studio 17 2022") {
        $cmakeArgs += "-A", "x64"
    }
    else {
        $cmakeArgs += "-DCMAKE_BUILD_TYPE=$($script:BuildType)"
    }

    if ($script:BuildTests) {
        $cmakeArgs += "-DBUILD_TESTS=ON"
    }
    else {
        $cmakeArgs += "-DBUILD_TESTS=OFF"
    }

    $cmakeArgs += $script:CMakeExtra
    
    # Note: CMAKE_BUILD_TYPE is ignored by multi-config generators like Visual Studio
    # Build type is specified during build phase with --config

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
    
    # Determine if using multi-config generator
    $isMultiConfig = ($script:Generator -eq "Visual Studio 17 2022")
    
    # Copy Qt DLLs next to executables so they can run without PATH setup
    $qtBinPath = $null
    foreach ($arg in $script:CMakeExtra) {
        if ($arg -match "-DCMAKE_PREFIX_PATH=(.+)") {
            $qtBinPath = Join-Path $Matches[1] "bin"
            break
        }
    }
    
    if ($qtBinPath -and (Test-Path $qtBinPath)) {
        $outputDirs = @()
        if ($script:BuildApp) {
            $outputDirs += if ($isMultiConfig) { "$buildDir\src\$($script:BuildType)" } else { "$buildDir\src" }
        }
        if ($script:BuildTests) {
            $outputDirs += if ($isMultiConfig) { "$buildDir\tests\$($script:BuildType)" } else { "$buildDir\tests" }
        }
        
        $qtDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6OpenGL.dll", "Qt6OpenGLWidgets.dll", "Qt6Network.dll")
        foreach ($dir in $outputDirs) {
            if (Test-Path $dir) {
                foreach ($dll in $qtDlls) {
                    $src = Join-Path $qtBinPath $dll
                    $dst = Join-Path $dir $dll
                    if ((Test-Path $src) -and !(Test-Path $dst)) {
                        Copy-Item $src $dst
                    }
                }
                # Copy Qt platform plugin
                $platformSrc = Join-Path (Split-Path $qtBinPath) "plugins\platforms\qwindows.dll"
                $platformDir = Join-Path $dir "platforms"
                if ((Test-Path $platformSrc) -and !(Test-Path $platformDir)) {
                    New-Item -ItemType Directory -Path $platformDir | Out-Null
                    Copy-Item $platformSrc (Join-Path $platformDir "qwindows.dll")
                }
                # Copy MinGW runtime DLLs if using MinGW
                $mingwBin = $null
                # qtBinPath is e.g. C:\Qt\6.10.2\mingw_64\bin -> Qt root is 3 levels up
                $qtRoot = Split-Path (Split-Path (Split-Path $qtBinPath))
                $mingwTools = Get-ChildItem "$qtRoot\Tools\mingw*\bin" -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($mingwTools) { $mingwBin = $mingwTools.FullName }
                if ($mingwBin) {
                    foreach ($rt in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
                        $src = Join-Path $mingwBin $rt
                        $dst = Join-Path $dir $rt
                        if ((Test-Path $src) -and !(Test-Path $dst)) {
                            Copy-Item $src $dst
                        }
                    }
                }
            }
        }
        Write-Info "Qt DLLs copied to output directories."
    }
    
    if ($script:BuildApp) {
        if ($isMultiConfig) {
            Write-Info "Executable: $buildDir\src\$($script:BuildType)\mcskin_raytracer.exe"
        } else {
            Write-Info "Executable: $buildDir\src\mcskin_raytracer.exe"
        }
    }
    if ($script:BuildTests) {
        if ($isMultiConfig) {
            Write-Info "Tests: $buildDir\tests\$($script:BuildType)\mcskin_tests.exe"
        } else {
            Write-Info "Tests: $buildDir\tests\mcskin_tests.exe"
        }
        Write-Host ""
        Write-Ask "Run tests now? [y/N] "
        $rt = Read-Host
        if ($rt -match "^[Yy]") {
            Write-Info "Running tests..."
            
            Push-Location $buildDir
            $testExe = if ($isMultiConfig) {
                Join-Path $buildDir "tests\$($script:BuildType)\mcskin_tests.exe"
            } else {
                Join-Path $buildDir "tests\mcskin_tests.exe"
            }
            
            if (Test-Path $testExe) {
                # Suppress PowerShell stderr error detection
                $ErrorActionPreference = "Continue"
                & $testExe --gtest_color=yes
                $testResult = $LASTEXITCODE
                $ErrorActionPreference = "Stop"
            } else {
                Write-Err "Test executable not found: $testExe"
                $testResult = 1
            }
            Pop-Location
            
            if ($testResult -ne 0) {
                Write-Warn "Some tests failed. Check output above for details."
            }
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
