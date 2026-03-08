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

function Resolve-Python {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        return @("py", "-3")
    }
    if (Get-Command python -ErrorAction SilentlyContinue) {
        return @("python")
    }
    throw "Missing required command: python"
}

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

$LibsDir = Join-Path $RepoRoot "libs"
New-Item -ItemType File -Force -Path (Join-Path $LibsDir ".gitkeep") | Out-Null

$ModuleLibDir = Join-Path $RepoRoot "module\system\lib64"
New-Item -ItemType Directory -Force -Path $ModuleLibDir | Out-Null
Copy-Item (Join-Path $RepoRoot "libs\arm64-v8a\libbabix_payload.so") (Join-Path $ModuleLibDir "libbabix_payload.so") -Force

$ModuleZygiskDir = Join-Path $RepoRoot "module\zygisk"
New-Item -ItemType Directory -Force -Path $ModuleZygiskDir | Out-Null
Copy-Item (Join-Path $RepoRoot "libs\arm64-v8a\libbabix_zygisk.so") (Join-Path $ModuleZygiskDir "arm64-v8a.so") -Force

Write-Host "[build] done: $RepoRoot\libs\arm64-v8a\libbabix_payload.so"
