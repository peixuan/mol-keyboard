# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,

    [Parameter()]
    [string] $Arguments = '',

    [Parameter()]
    [string] $StartupDirectory = ''
)

$ErrorActionPreference = 'Stop'
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
if ([IO.Path]::GetExtension($resolvedExecutable) -ne '.exe') {
    throw 'Executable must point to mol-keyboardd.exe.'
}

$startup = if ([string]::IsNullOrWhiteSpace($StartupDirectory)) {
    [Environment]::GetFolderPath([Environment+SpecialFolder]::Startup)
} else {
    (Resolve-Path -LiteralPath $StartupDirectory -ErrorAction Stop).Path
}
if ([string]::IsNullOrWhiteSpace($startup)) {
    throw 'The current user Startup folder is unavailable.'
}
if (-not (Test-Path -LiteralPath $startup -PathType Container)) {
    throw 'The selected Startup folder is not a directory.'
}

$shortcutPath = Join-Path $startup 'MoL Keyboard Service.lnk'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $resolvedExecutable
$shortcut.Arguments = $Arguments
$shortcut.WorkingDirectory = Split-Path -Parent $resolvedExecutable
$shortcut.Description = 'Start the MoL Keyboard headless service for this user.'
$shortcut.WindowStyle = 7
$shortcut.Save()

Write-Output "Installed current-user startup shortcut: $shortcutPath"
