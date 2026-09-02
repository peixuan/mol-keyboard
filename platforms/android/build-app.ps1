# SPDX-License-Identifier: Apache-2.0
param(
    [ValidateSet("Debug", "Release", "DebugAndroidTest")]
    [string]$Variant = "Debug"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if (-not $env:EMSDK) {
    throw "EMSDK must point to the pinned Emscripten SDK."
}
if (-not $env:ANDROID_HOME -and -not $env:ANDROID_SDK_ROOT) {
    throw "ANDROID_HOME or ANDROID_SDK_ROOT must point to Android SDK 36."
}

if ($env:MOL_SKIP_WEB_BUILD -ne "1") {
    Push-Location $repositoryRoot
    try {
        & cmake --preset wasm-release
        if ($LASTEXITCODE -ne 0) { throw "Wasm configuration failed." }
        & cmake --build --preset wasm-release
        if ($LASTEXITCODE -ne 0) { throw "Wasm build failed." }
        & ctest --preset wasm-release
        if ($LASTEXITCODE -ne 0) { throw "Wasm tests failed." }
    } finally {
        Pop-Location
    }

    Push-Location (Join-Path $repositoryRoot "apps\web")
    try {
        & npm ci
        if ($LASTEXITCODE -ne 0) { throw "Web dependency installation failed." }
        & npm test
        if ($LASTEXITCODE -ne 0) { throw "Web unit tests failed." }
        & npm run build
        if ($LASTEXITCODE -ne 0) { throw "Web production build failed." }
    } finally {
        Pop-Location
    }
}

$gradleArguments = @(":app:assemble$Variant", "--no-daemon", "--no-configuration-cache")
if ($env:MOL_ANDROID_AAPT2_OVERRIDE) {
    $gradleArguments += "-Pandroid.aapt2FromMavenOverride=$env:MOL_ANDROID_AAPT2_OVERRIDE"
}

Push-Location $PSScriptRoot
try {
    & ".\gradlew.bat" @gradleArguments
    if ($LASTEXITCODE -ne 0) { throw "Android $Variant build failed." }
} finally {
    Pop-Location
}
