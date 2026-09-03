# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("esp32", "esp32s3")]
  [string] $Target,

  [switch] $WebUi
)

$buildDirectory = if ($WebUi) { "build-$Target-web" } else { "build-$Target" }

Push-Location $PSScriptRoot
try {
  if ($WebUi) {
    $defaults = "sdkconfig.defaults;sdkconfig.defaults.web"
    & idf.py -B $buildDirectory "-DSDKCONFIG_DEFAULTS=$defaults" `
      "-DMOL_DEVICE_WEB_COMPONENTS=ON" set-target $Target
  }
  else {
    & idf.py -B $buildDirectory set-target $Target
  }

  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }

  & idf.py -B $buildDirectory build
  exit $LASTEXITCODE
}
finally {
  Pop-Location
}
