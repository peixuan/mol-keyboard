# SPDX-License-Identifier: Apache-2.0
param(
    [ValidateSet("Debug", "Release")]
    [string]$Variant = "Release"
)

$ErrorActionPreference = "Stop"
$projectRoot = Join-Path $PSScriptRoot "app"
$sourceMain = Join-Path $projectRoot "entry\src\main"
$compatMain = Join-Path $projectRoot "entry-openharmony\src\main"

if (-not $env:HVIGORW) {
    throw "HVIGORW must point to an OpenHarmony-compatible hvigorw executable."
}
if (-not (Test-Path -LiteralPath $env:HVIGORW -PathType Leaf)) {
    throw "HVIGORW does not exist: $env:HVIGORW"
}
if (-not $env:OHOS_BASE_SDK_HOME) {
    throw "OHOS_BASE_SDK_HOME must point to an API 12 or newer OpenHarmony SDK root."
}
if (-not (Test-Path -LiteralPath $env:OHOS_BASE_SDK_HOME -PathType Container)) {
    throw "OHOS_BASE_SDK_HOME does not exist: $env:OHOS_BASE_SDK_HOME"
}
if (-not (Get-Command java -ErrorAction SilentlyContinue)) {
    throw "A JDK must be available on PATH for HAP packaging."
}
if (-not (Get-Command tar -ErrorAction SilentlyContinue)) {
    throw "tar is required to audit the HAP contents."
}

$projectPrefix = [IO.Path]::GetFullPath($projectRoot) + [IO.Path]::DirectorySeparatorChar
$compatPath = [IO.Path]::GetFullPath($compatMain) + [IO.Path]::DirectorySeparatorChar
if (-not $compatPath.StartsWith($projectPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Compatibility source directory escaped the Harmony project."
}

foreach ($name in @("ets", "resources", "cpp")) {
    $source = Join-Path $sourceMain $name
    $destination = Join-Path $compatMain $name
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Shared Harmony source directory is missing: $source"
    }
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -Path (Join-Path $source "*") -Destination $destination -Recurse -Force
}

$buildMode = $Variant.ToLowerInvariant()
Push-Location $projectRoot
try {
    & $env:HVIGORW assembleHap --mode module `
        -p product=openharmony `
        -p module=entryOpenHarmony@default `
        -p buildMode=$buildMode `
        --no-daemon
    if ($LASTEXITCODE -ne 0) {
        throw "OpenHarmony compatibility $Variant build failed."
    }
} finally {
    Pop-Location
}

$hap = Get-ChildItem -LiteralPath (Join-Path $projectRoot "entry-openharmony\build") `
    -Recurse -Filter "*.hap" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $hap) {
    throw "Hvigor completed without producing an OpenHarmony compatibility HAP."
}

$contents = @(& tar -tf $hap.FullName)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect HAP contents: $($hap.FullName)"
}
foreach ($required in @(
    "module.json",
    "resources.index",
    "ets/modules.abc",
    "libs/arm64-v8a/libmol_harmony_audio.so",
    "libs/x86_64/libmol_harmony_audio.so"
)) {
    if ($contents -notcontains $required) {
        throw "HAP is missing required entry: $required"
    }
}

$hash = (Get-FileHash -LiteralPath $hap.FullName -Algorithm SHA256).Hash
Write-Output "OpenHarmony compatibility HAP: $($hap.FullName)"
Write-Output "SHA256: $hash"
