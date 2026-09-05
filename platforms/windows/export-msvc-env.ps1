# SPDX-License-Identifier: Apache-2.0

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$EnvironmentFile
)

$ErrorActionPreference = 'Stop'

$programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "Visual Studio locator not found: $vswhere"
}

$installationPath = & $vswhere `
    -latest `
    -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installationPath)) {
    throw 'A Visual Studio installation with the x64 C++ toolchain was not found.'
}
$installationPath = $installationPath.Trim()

$devShellModule = Join-Path $installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
    throw "Visual Studio developer-shell module not found: $devShellModule"
}
Import-Module $devShellModule
Enter-VsDevShell `
    -VsInstallPath $installationPath `
    -SkipAutomaticLocation `
    -DevCmdArguments '-arch=amd64 -host_arch=amd64'

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $bundledNinja = Join-Path $installationPath `
        'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    if (-not (Test-Path -LiteralPath $bundledNinja -PathType Leaf)) {
        throw 'Ninja is neither on PATH nor installed with Visual Studio.'
    }
    $env:Path = "$(Split-Path -Parent $bundledNinja);$env:Path"
}

foreach ($tool in @('cl', 'link', 'ninja', 'cmake')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Required MSVC build tool is unavailable after activation: $tool"
    }
}

$environmentNames = @(
    'Path',
    'INCLUDE',
    'LIB',
    'LIBPATH',
    'UCRTVersion',
    'UniversalCRTSdkDir',
    'VCINSTALLDIR',
    'VCToolsInstallDir',
    'VCToolsRedistDir',
    'VSCMD_ARG_HOST_ARCH',
    'VSCMD_ARG_TGT_ARCH',
    'VSCMD_VER',
    'VSINSTALLDIR',
    'WindowsLibPath',
    'WindowsSdkBinPath',
    'WindowsSdkDir',
    'WindowsSDKVersion'
)

$resolvedEnvironmentFile = [IO.Path]::GetFullPath($EnvironmentFile)
$parent = Split-Path -Parent $resolvedEnvironmentFile
if (-not [string]::IsNullOrEmpty($parent)) {
    [IO.Directory]::CreateDirectory($parent) | Out-Null
}
foreach ($name in $environmentNames) {
    $value = [Environment]::GetEnvironmentVariable($name, 'Process')
    if (-not [string]::IsNullOrEmpty($value)) {
        Add-Content `
            -LiteralPath $resolvedEnvironmentFile `
            -Value "$name=$value" `
            -Encoding utf8
    }
}

$compiler = (Get-Command cl).Source
$ninja = (Get-Command ninja).Source
Write-Host "Activated MSVC from $installationPath"
Write-Host "Compiler: $compiler"
Write-Host "Ninja: $ninja"
