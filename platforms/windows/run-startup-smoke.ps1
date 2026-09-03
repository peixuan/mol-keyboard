# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Daemon,

    [Parameter(Mandatory = $true)]
    [string] $Controller
)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') {
    throw 'The Windows Startup smoke requires Windows.'
}

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$installer = Join-Path $repositoryRoot 'packaging\windows\install-user-startup.ps1'
$uninstaller = Join-Path $repositoryRoot 'packaging\windows\uninstall-user-startup.ps1'
$resolvedDaemon = (Resolve-Path -LiteralPath $Daemon -ErrorAction Stop).Path
$resolvedController = (Resolve-Path -LiteralPath $Controller -ErrorAction Stop).Path
if ([IO.Path]::GetExtension($resolvedDaemon) -ne '.exe' -or
    [IO.Path]::GetExtension($resolvedController) -ne '.exe') {
    throw 'Built Windows mol-keyboardd and molctl executables are required.'
}

$identifier = '{0}-{1}' -f $PID, ([Guid]::NewGuid().ToString('N'))
$artifactDirectory = Join-Path ([IO.Path]::GetTempPath()) "mol-keyboard-startup-$identifier"
$stateDirectory = Join-Path $artifactDirectory 'state'
$recording = Join-Path $artifactDirectory 'take.molseq'
$endpoint = "\\.\pipe\mol-keyboard-startup-$identifier"
$shortcutPath = Join-Path $artifactDirectory 'MoL Keyboard Service.lnk'
$serviceProcess = $null
$shortcutInstalled = $false

function Invoke-MolController {
    param([Parameter(Mandatory = $true)][string[]] $CommandArguments)

    $arguments = @('--json', '--endpoint', $endpoint, '--state-dir', $stateDirectory) +
        $CommandArguments
    $text = (& $resolvedController @arguments) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "molctl failed with exit code $LASTEXITCODE for $($CommandArguments -join ' ')"
    }
    $document = $text | ConvertFrom-Json
    if ($null -ne $document.error -or $null -eq $document.result) {
        throw "molctl returned an invalid or failed response for $($CommandArguments -join ' ')"
    }
    return $document.result
}

try {
    New-Item -ItemType Directory -Path $artifactDirectory -ErrorAction Stop | Out-Null
    $daemonArguments = '--null-backend --state-dir "{0}" --endpoint "{1}"' -f $stateDirectory, $endpoint
    & $installer -Executable $resolvedDaemon -Arguments $daemonArguments -StartupDirectory $artifactDirectory
    $shortcutInstalled = $true
    if (-not (Test-Path -LiteralPath $shortcutPath -PathType Leaf)) {
        throw 'The current-user startup shortcut was not created.'
    }

    $shell = New-Object -ComObject WScript.Shell
    try {
        $shortcut = $shell.CreateShortcut($shortcutPath)
        if ($shortcut.TargetPath -ne $resolvedDaemon -or
            $shortcut.Arguments -ne $daemonArguments -or
            $shortcut.WorkingDirectory -ne (Split-Path -Parent $resolvedDaemon) -or
            $shortcut.WindowStyle -ne 7) {
            throw 'The startup shortcut contract is invalid.'
        }
    } finally {
        if ($null -ne $shell) {
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
        }
    }

    $existingProcessIds = @(Get-Process -Name 'mol-keyboardd' -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Id)
    $launchedProcess = Start-Process -FilePath $shortcutPath -WindowStyle Hidden -PassThru
    if ($launchedProcess.ProcessName -eq 'mol-keyboardd') {
        $serviceProcess = $launchedProcess
    } else {
        for ($attempt = 0; $attempt -lt 100; ++$attempt) {
            $candidate = Get-Process -Name 'mol-keyboardd' -ErrorAction SilentlyContinue |
                Where-Object { $_.Id -notin $existingProcessIds } |
                Select-Object -First 1
            if ($null -ne $candidate) {
                $serviceProcess = $candidate
                break
            }
            Start-Sleep -Milliseconds 50
        }
    }
    if ($null -eq $serviceProcess) {
        throw 'The startup shortcut did not create a daemon process.'
    }
    # Open and retain the process handle before RPC shutdown so ExitCode remains
    # queryable even when the process exits quickly.
    [void]$serviceProcess.Handle

    $ready = $false
    $statusArguments = @('--json', '--endpoint', $endpoint, '--state-dir', $stateDirectory, 'status')
    for ($attempt = 0; $attempt -lt 100; ++$attempt) {
        $statusText = (& $resolvedController @statusArguments 2>$null) -join "`n"
        if ($LASTEXITCODE -eq 0) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 50
    }
    if (-not $ready) {
        throw 'The startup-shortcut daemon did not become ready.'
    }

    $status = $statusText | ConvertFrom-Json
    if ($status.result.sample_rate -ne 48000 -or $status.result.channel_count -ne 2) {
        throw 'The startup-shortcut daemon engine state is invalid.'
    }
    [void](Invoke-MolController @('capabilities'))
    [void](Invoke-MolController @('preset', 'set', 'violin'))
    [void](Invoke-MolController @('tempo', '123'))
    [void](Invoke-MolController @('record', 'start'))
    [void](Invoke-MolController @('note', 'on', '60', '--velocity', '0.8', '--gesture', '7003'))
    Start-Sleep -Milliseconds 100
    [void](Invoke-MolController @('note', 'off', '--gesture', '7003'))
    [void](Invoke-MolController @('record', 'stop', '--output', $recording))
    if (-not (Test-Path -LiteralPath $recording -PathType Leaf) -or
        (Get-Item -LiteralPath $recording).Length -eq 0) {
        throw 'The startup-shortcut daemon did not persist its recording.'
    }
    [void](Invoke-MolController @('play', $recording))
    [void](Invoke-MolController @('rpc', 'playback.stop', '{}'))
    $audio = Invoke-MolController @('rpc', 'audio.getLatency', '{}')
    $selfTest = Invoke-MolController @('self-test')
    $doctor = Invoke-MolController @('doctor')
    # Windows PowerShell 5.1 reconstructs a native command line. Backslashes
    # preserve the JSON quotes through CommandLineToArgvW/CRT parsing.
    $benchmark = Invoke-MolController @('rpc', 'diagnostics.benchmark', '{\"frames\":4096}')
    if ($audio.backend -ine 'Null' -or -not $audio.null_sink) {
        throw 'The startup-shortcut daemon did not use the null backend.'
    }
    if (-not $selfTest.ok -or -not $doctor.ok -or $benchmark.frames -ne 4096 -or
        $benchmark.non_finite_samples -ne 0) {
        throw 'The startup-shortcut daemon diagnostics failed.'
    }
    [void](Invoke-MolController @('all-notes-off'))
    [void](Invoke-MolController @('shutdown'))

    if (-not $serviceProcess.WaitForExit(5000)) {
        throw 'The startup-shortcut daemon did not exit after shutdown.'
    }
    $serviceProcess.Refresh()
    if ($null -eq $serviceProcess.ExitCode -or $serviceProcess.ExitCode -ne 0) {
        throw "The startup-shortcut daemon exited with $($serviceProcess.ExitCode)."
    }

    & $uninstaller -StartupDirectory $artifactDirectory
    $shortcutInstalled = $false
    if (Test-Path -LiteralPath $shortcutPath) {
        throw 'The startup shortcut remains after uninstall.'
    }
    Write-Output ($status | ConvertTo-Json -Depth 8)
    Write-Output ($audio | ConvertTo-Json -Depth 8)
    Write-Output ($selfTest | ConvertTo-Json -Depth 8)
    Write-Output ($doctor | ConvertTo-Json -Depth 8)
    Write-Output ($benchmark | ConvertTo-Json -Depth 8)
    Write-Output 'MOL_WINDOWS_STARTUP_SMOKE_PASS'
} finally {
    if ($null -ne $serviceProcess -and -not $serviceProcess.HasExited) {
        Stop-Process -Id $serviceProcess.Id -Force -ErrorAction SilentlyContinue
        [void]$serviceProcess.WaitForExit(5000)
    }
    if ($shortcutInstalled -and (Test-Path -LiteralPath $artifactDirectory -PathType Container)) {
        & $uninstaller -StartupDirectory $artifactDirectory
    }
    if (Test-Path -LiteralPath $artifactDirectory -PathType Container) {
        $resolvedArtifact = (Resolve-Path -LiteralPath $artifactDirectory).Path
        $expectedPrefix = Join-Path ([IO.Path]::GetTempPath()) 'mol-keyboard-startup-'
        if (-not $resolvedArtifact.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to remove an unexpected startup smoke artifact path.'
        }
        Remove-Item -LiteralPath $resolvedArtifact -Recurse -Force
    }
}
