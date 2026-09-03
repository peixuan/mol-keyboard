# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet("esp32", "esp32s3")]
  [string] $Target,

  [switch] $WebUi,
  [switch] $Qemu
)

if ($WebUi -and $Qemu) {
  throw "-WebUi and -Qemu are mutually exclusive"
}

$buildDirectory = if ($WebUi) {
  "build-$Target-web"
}
elseif ($Qemu) {
  "build-$Target-qemu"
}
else {
  "build-$Target"
}

Push-Location $PSScriptRoot
try {
  if ($WebUi) {
    $defaults = "sdkconfig.defaults;sdkconfig.defaults.web"
    & idf.py -B $buildDirectory "-DSDKCONFIG_DEFAULTS=$defaults" `
      "-DMOL_DEVICE_WEB_COMPONENTS=ON" set-target $Target
  }
  elseif ($Qemu) {
    $defaults = "sdkconfig.defaults;sdkconfig.defaults.qemu"
    $sdkconfigPath = Join-Path $buildDirectory "sdkconfig"
    $configured = $false
    if (Test-Path -LiteralPath $sdkconfigPath) {
      $sdkconfigText = Get-Content -Raw -LiteralPath $sdkconfigPath
      $configured = $sdkconfigText.Contains("CONFIG_IDF_TARGET=`"$Target`"") -and
        $sdkconfigText.Contains("CONFIG_MOL_QEMU_RUNTIME=y")
    }
    if ($configured) {
      Write-Host "Reusing configured $Target QEMU build directory"
    }
    else {
      & idf.py -B $buildDirectory "-DSDKCONFIG_DEFAULTS=$defaults" set-target $Target
    }
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
