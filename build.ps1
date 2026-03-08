$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $true

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SdkRoot = if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA "Android\Sdk" } else { $null }

function Resolve-NdkRoot {
    $candidates = @(
        $env:ANDROID_NDK_ROOT,
        $env:ANDROID_NDK_HOME,
        $env:NDK_ROOT
    ) | Where-Object { $_ -and (Test-Path $_) }

    if ($candidates.Count -gt 0) {
        return $candidates[0]
    }

    if ($env:LOCALAPPDATA) {
        $sdkRoot = Join-Path $env:LOCALAPPDATA "Android\Sdk\ndk"
        if (Test-Path $sdkRoot) {
            $latest = Get-ChildItem $sdkRoot -Directory | Sort-Object Name | Select-Object -Last 1
            if ($latest) {
                return $latest.FullName
            }
        }
    }

    throw "Unable to resolve Android NDK root. Set ANDROID_NDK_ROOT or ANDROID_NDK_HOME."
}

function Resolve-SdkBinary {
    param(
        [string]$CommandName,
        [string]$RelativeSdkPath
    )

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    if ($SdkRoot) {
        $candidate = Join-Path $SdkRoot $RelativeSdkPath
        if (Test-Path $candidate) {
            return $candidate
        }

        $glob = Join-Path $SdkRoot "cmake\*\bin\$CommandName.exe"
        $latest = Get-ChildItem $glob -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1
        if ($latest) {
            return $latest.FullName
        }
    }

    throw "Missing required tool: $CommandName"
}

function Resolve-Python {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        return @("py", "-3")
    }
    if (Get-Command python -ErrorAction SilentlyContinue) {
        return @("python")
    }
    throw "Missing required command: python"
}

$Cmake = Resolve-SdkBinary -CommandName "cmake" -RelativeSdkPath "cmake\3.22.1\bin\cmake.exe"
$Ninja = Resolve-SdkBinary -CommandName "ninja" -RelativeSdkPath "cmake\3.22.1\bin\ninja.exe"
$env:Path = "$(Split-Path $Ninja -Parent);$env:Path"

$NdkRoot = Resolve-NdkRoot
$NdBuild = Join-Path $NdkRoot "ndk-build.cmd"
if (-not (Test-Path $NdBuild)) {
    $NdBuild = Join-Path $NdkRoot "ndk-build"
}
if (-not (Test-Path $NdBuild)) {
    throw "ndk-build not found under $NdkRoot"
}

$Python = Resolve-Python

Write-Host "[build] preparing BNM headers"
if ($Python.Count -gt 1) {
    & $Python[0] $Python[1] "$RepoRoot\scripts\prepare_bnm_headers.py"
} else {
    & $Python[0] "$RepoRoot\scripts\prepare_bnm_headers.py"
}

$DobbyBuildDir = Join-Path $RepoRoot "out\dobby\android-arm64"
$DobbyPrebuiltDir = Join-Path $RepoRoot "jni\external\Dobby\prebuilt\arm64-v8a"

Write-Host "[build] configuring Dobby"
& $Cmake `
    -S "$RepoRoot\jni\external\Dobby" `
    -B $DobbyBuildDir `
    -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="$NdkRoot\build\cmake\android.toolchain.cmake" `
    -DANDROID_ABI=arm64-v8a `
    -DANDROID_PLATFORM=26 `
    -DANDROID_STL=c++_static `
    -DDOBBY_DEBUG=OFF `
    "-DPlugin.SymbolResolver=OFF" `
    "-DPlugin.ImportTableReplace=OFF" `
    "-DPlugin.Android.BionicLinkerUtil=OFF" `
    -DDOBBY_BUILD_EXAMPLE=OFF `
    -DDOBBY_BUILD_TEST=OFF

Write-Host "[build] building Dobby static library"
& $Cmake --build $DobbyBuildDir --target dobby_static --parallel

New-Item -ItemType Directory -Force -Path $DobbyPrebuiltDir | Out-Null
Copy-Item (Join-Path $DobbyBuildDir "libdobby.a") (Join-Path $DobbyPrebuiltDir "libdobby.a") -Force

if (Test-Path (Join-Path $RepoRoot "libs")) {
    Remove-Item (Join-Path $RepoRoot "libs") -Recurse -Force
}
if (Test-Path (Join-Path $RepoRoot "obj")) {
    Remove-Item (Join-Path $RepoRoot "obj") -Recurse -Force
}

Write-Host "[build] building payload with ndk-build"
& $NdBuild `
    -C $RepoRoot `
    "NDK_PROJECT_PATH=$RepoRoot" `
    "NDK_APPLICATION_MK=$RepoRoot\jni\Application.mk" `
    "-j$([Environment]::ProcessorCount)"

$ModuleLibDir = Join-Path $RepoRoot "module\system\lib64"
New-Item -ItemType Directory -Force -Path $ModuleLibDir | Out-Null
Copy-Item (Join-Path $RepoRoot "libs\arm64-v8a\libbabix_payload.so") (Join-Path $ModuleLibDir "libbabix_payload.so") -Force

Write-Host "[build] done: $RepoRoot\libs\arm64-v8a\libbabix_payload.so"
