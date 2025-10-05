<#
build.ps1 - Compile src\main.cpp using clang++ and link OpenGL, GLEW, and GLFW

Usage:
  .\build.ps1            # build
  .\build.ps1 -Clean     # remove build directory

Environment variables (optional):
  GLEW_DIR  - path to GLEW installation (contains include/ and lib/)
  GLFW_DIR  - path to GLFW installation (contains include/ and lib/)
#>

param(
    [switch]$Clean,
    [switch]$Test,
    [string]$OutDir = 'build'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$here = Split-Path -Path $MyInvocation.MyCommand.Definition -Parent
$projRoot = Resolve-Path "$here"

$outDirFull = Join-Path $projRoot $OutDir

if ($Clean) {
    if (Test-Path $outDirFull) {
        Write-Host "Removing directory $outDirFull"
        Remove-Item $outDirFull -Recurse -Force
    }
    exit 0
}

if ($Test) {
    $testSrcFile = 'test_math.cpp'
    $testOutExe = Join-Path $outDirFull 'test_runner.exe'

    Write-Host "Building and running tests..."
    if (-not (Test-Path $outDirFull)) {
        New-Item -ItemType Directory -Path $outDirFull | Out-Null
    }

    $clang = Get-Command clang++ -ErrorAction SilentlyContinue
    if (-not $clang) {
        Write-Error "clang++ not found in PATH. Please install clang and try again."
        exit 1
    }

    Set-Location (Join-Path $projRoot 'tests')

    $testArgs = @('-std=c++17', '-O2', "`"$testSrcFile`"", '-o', "`"$testOutExe`"")
    Write-Host "clang++ $($testArgs -join ' ')"
    & clang++ @testArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Test compilation failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    Write-Host "`nRunning tests..."
    & $testOutExe
    exit 0
}

$src = Join-Path $projRoot 'src\main.cpp'

if (-not (Test-Path $src)) {
    Write-Error "Source file not found: $src"
    exit 1
}

# Create output directory
if (-not (Test-Path $outDirFull)) {
    New-Item -ItemType Directory -Path $outDirFull | Out-Null
}

$outExe = Join-Path $outDirFull 'a.exe'

# Auto-detect libraries in %USERPROFILE%\lib if env vars not set
if (-not $env:GLEW_DIR) {
    $userLib = Join-Path $env:USERPROFILE 'lib'
    if (Test-Path $userLib) {
        $found = Get-ChildItem -Path $userLib -Directory -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -match '(?i)glew' } |
                 Select-Object -First 1
        if ($found) {
            $env:GLEW_DIR = $found.FullName
            Write-Host "Auto-detected GLEW_DIR = $($env:GLEW_DIR)"
        }
    }
}

if (-not $env:GLFW_DIR) {
    $userLib = Join-Path $env:USERPROFILE 'lib'
    if (Test-Path $userLib) {
        $found = Get-ChildItem -Path $userLib -Directory -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -match '(?i)glfw' } |
                 Select-Object -First 1
        if ($found) {
            $env:GLFW_DIR = $found.FullName
            Write-Host "Auto-detected GLFW_DIR = $($env:GLFW_DIR)"
        }
    }
}

# Check for clang++
$clang = Get-Command clang++ -ErrorAction SilentlyContinue
if (-not $clang) {
    Write-Error "clang++ not found in PATH. Please install clang and try again."
    exit 1
}

Write-Host "Using compiler: $($clang.Source)"

# Build include and lib flags
$includeFlags = @()
$libFlags = @()

if ($env:GLEW_DIR) {
    $glewInc = Join-Path $env:GLEW_DIR 'include'
    $glewLib = Join-Path $env:GLEW_DIR 'lib'
    if (Test-Path $glewInc) { $includeFlags += "-I`"$glewInc`"" }
    if (Test-Path $glewLib) { $libFlags += "-L`"$glewLib`"" }
}

if ($env:GLFW_DIR) {
    $glfwInc = Join-Path $env:GLFW_DIR 'include'
    if (Test-Path $glfwInc) { $includeFlags += "-I`"$glfwInc`"" }

    # Find GLFW lib directory (try common subdirectories)
    $glfwLibDirs = @(
        (Join-Path $env:GLFW_DIR 'lib'),
        (Join-Path $env:GLFW_DIR 'lib-vc2022'),
        (Join-Path $env:GLFW_DIR 'lib-vc2019'),
        (Join-Path $env:GLFW_DIR 'lib-mingw-w64')
    )
    foreach ($dir in $glfwLibDirs) {
        if (Test-Path $dir) {
            $libFlags += "-L`"$dir`""
            break
        }
    }
}

# Check if we need to compile GLEW from source
$glewSrc = $null
$glewLib = $null
if ($env:GLEW_DIR) {
    $glewSrcPath = Join-Path $env:GLEW_DIR 'src\glew.c'
    if (Test-Path $glewSrcPath) {
        $glewSrc = $glewSrcPath
        Write-Host "Found GLEW source, will compile from source"
    }
}

if ($glewSrc) {
    # Compile GLEW and main.cpp separately, then link
    $glewObj = Join-Path $outDirFull 'glew.o'
    $mainObj = Join-Path $outDirFull 'main.o'

    Write-Host "`nCompiling GLEW..."
    $glewArgs = @('-c', '-O2', '-DGLEW_STATIC') + $includeFlags + @("`"$glewSrc`"", '-o', "`"$glewObj`"")
    Write-Host "clang $($glewArgs -join ' ')"
    & clang @glewArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Error "GLEW compilation failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    Write-Host "`nCompiling main.cpp..."
    $mainArgs = @('-std=c++17', '-c', '-O2', '-DGLEW_STATIC') + $includeFlags + @("`"$src`"", '-o', "`"$mainObj`"")
    Write-Host "clang++ $($mainArgs -join ' ')"
    & clang++ @mainArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Error "main.cpp compilation failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    Write-Host "`nLinking..."
    $linkArgs = @("`"$mainObj`"", "`"$glewObj`"", '-o', "`"$outExe`"") + $libFlags + @(
        '-lglfw3'
        '-lopengl32'
        '-lgdi32'
        '-luser32'
        '-lkernel32'
        '-lshell32'
        '-Xlinker'
        '/NODEFAULTLIB:libcmt'
    )
    Write-Host "clang++ $($linkArgs -join ' ')"
    & clang++ @linkArgs
} else {
    # Try linking with prebuilt library
    $compileArgs = @(
        '-std=c++17'
        '-O2'
    ) + $includeFlags + @(
        "`"$src`""
        '-o'
        "`"$outExe`""
    ) + $libFlags + @(
        '-lglew32'
        '-lglfw3'
        '-lopengl32'
        '-lgdi32'
        '-luser32'
        '-lkernel32'
    )

    Write-Host "`nCompiling..."
    Write-Host "clang++ $($compileArgs -join ' ')"

    # Run clang++ directly in this process
    & clang++ @compileArgs
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "`nBuild successful: $outExe"

# Copy DLLs to build directory
Write-Host "`nCopying DLLs..."
$dllsCopied = 0

function Copy-DllsFrom {
    param($baseDir, $name)
    if (-not $baseDir -or -not (Test-Path $baseDir)) { return }

    $searchDirs = @(
        (Join-Path $baseDir 'bin'),
        (Join-Path $baseDir 'lib')
    )

    foreach ($dir in $searchDirs) {
        if (Test-Path $dir) {
            Get-ChildItem -Path $dir -Filter '*.dll' -File -ErrorAction SilentlyContinue | ForEach-Object {
                $dest = Join-Path $outDirFull $_.Name
                Write-Host "  Copying $($_.Name) from $name"
                Copy-Item -Path $_.FullName -Destination $dest -Force
                $script:dllsCopied++
            }
        }
    }
}

Copy-DllsFrom -baseDir $env:GLEW_DIR -name 'GLEW'
Copy-DllsFrom -baseDir $env:GLFW_DIR -name 'GLFW'

if ($dllsCopied -eq 0) {
    Write-Host "  No DLLs found (you may need to copy them manually)"
}

Write-Host "`nDone!"
exit 0
