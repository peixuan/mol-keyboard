# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("esp32", "esp32s3")]
  [string] $Target,

  [Parameter(Mandatory = $true)]
  [string] $Port,

  [ValidateRange(1, 1440)]
  [int] $DurationMinutes = 30,

  [string] $I2sCaptureWav,

  [switch] $WebUi,
  [switch] $SerialOnly,
  [switch] $RequireGpio,
  [switch] $RequireBluetooth,
  [switch] $RequireUsb,
  [switch] $RequireA2dp,
  [switch] $RequireClearPairing
)

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$buildDirectory = if ($WebUi) { "build-$Target-web" } else { "build-$Target" }
$evidenceDirectory = Join-Path $repositoryRoot "build\hil-$Target"
$runner = Join-Path $repositoryRoot "tests\hardware\esp32_hil.py"

& (Join-Path $PSScriptRoot "build-target.ps1") -Target $Target -WebUi:$WebUi
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Push-Location $PSScriptRoot
try {
  & idf.py -B $buildDirectory -p $Port flash
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}
finally {
  Pop-Location
}

$arguments = @(
  $runner,
  "--target", $Target,
  "--port", $Port,
  "--duration-seconds", ($DurationMinutes * 60),
  "--report", (Join-Path $evidenceDirectory "report.json"),
  "--log", (Join-Path $evidenceDirectory "serial.log")
)
if ($I2sCaptureWav) { $arguments += @("--i2s-capture", $I2sCaptureWav) }
if ($SerialOnly) { $arguments += "--serial-only" }
if ($RequireGpio) { $arguments += "--require-gpio" }
if ($RequireBluetooth) { $arguments += "--require-bluetooth" }
if ($RequireUsb) { $arguments += "--require-usb" }
if ($RequireA2dp) { $arguments += "--require-a2dp" }
if ($WebUi) { $arguments += "--require-web" }
if ($RequireClearPairing) { $arguments += "--require-clear-pairing" }

& python @arguments
exit $LASTEXITCODE
