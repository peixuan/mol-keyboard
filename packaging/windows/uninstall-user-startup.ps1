# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
    [Parameter()]
    [string] $StartupDirectory = ''
)

$ErrorActionPreference = 'Stop'
$startup = if ([string]::IsNullOrWhiteSpace($StartupDirectory)) {
    [Environment]::GetFolderPath([Environment+SpecialFolder]::Startup)
} else {
    (Resolve-Path -LiteralPath $StartupDirectory -ErrorAction Stop).Path
}
if ([string]::IsNullOrWhiteSpace($startup) -or
    -not (Test-Path -LiteralPath $startup -PathType Container)) {
    throw 'The selected Startup folder is unavailable.'
}
$shortcutPath = Join-Path $startup 'MoL Keyboard Service.lnk'
if (Test-Path -LiteralPath $shortcutPath -PathType Leaf) {
    Remove-Item -LiteralPath $shortcutPath -Force
    Write-Output "Removed current-user startup shortcut: $shortcutPath"
} else {
    Write-Output 'MoL Keyboard current-user startup shortcut is not installed.'
}
