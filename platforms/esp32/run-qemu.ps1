# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("esp32", "esp32s3")]
  [string] $Target,

  [ValidateRange(30, 600)]
  [int] $TimeoutSeconds = 180
)

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$buildDirectory = Join-Path $PSScriptRoot "build-$Target-qemu"
$evidenceDirectory = Join-Path $repositoryRoot "build\qemu-$Target"
$runner = Join-Path $repositoryRoot "tests\hardware\esp32_qemu.py"

& (Join-Path $PSScriptRoot "build-target.ps1") -Target $Target -Qemu
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

& python $runner `
  --target $Target `
  --project-directory $PSScriptRoot `
  --build-directory $buildDirectory `
  --timeout-seconds $TimeoutSeconds `
  --report (Join-Path $evidenceDirectory "report.json") `
  --log (Join-Path $evidenceDirectory "serial.log")
exit $LASTEXITCODE
