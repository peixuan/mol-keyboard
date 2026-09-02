# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("esp32", "esp32s3")]
  [string] $Target
)

$buildDirectory = "build-$Target"

Push-Location $PSScriptRoot
try {
  & idf.py -B $buildDirectory set-target $Target
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }

  & idf.py -B $buildDirectory build
  exit $LASTEXITCODE
}
finally {
  Pop-Location
}
