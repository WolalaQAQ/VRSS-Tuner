param([ValidateSet('Baballonia','PSVR2Toolkit','SRanibro','ViveProEye','ViveStreaming','SteamVREye','Varjo')]
      [string]$InputRoute='Baballonia')
$ErrorActionPreference = 'Stop'
$bundle = $PSScriptRoot
$gameWasRunning = [bool](Get-Process VRChat -ErrorAction SilentlyContinue)
$gameClosed = $false
$providerStarted = $false
$gameLaunched = $false

function Wait-ForProcess([string]$name, [int]$seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    do {
        if (Get-Process $name -ErrorAction SilentlyContinue) { return $true }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)
    return $false
}

function Start-SteamApp([int]$appId) {
    Start-Process -FilePath ('steam://rungameid/' + $appId) | Out-Null
}

try {
    if ($gameWasRunning) {
        Write-Output 'Closing VRChat normally before starting the common gaze provider...'
        foreach ($game in @(Get-Process VRChat -ErrorAction SilentlyContinue)) {
            if (-not $game.CloseMainWindow()) {
                throw 'VRChat did not accept a normal close. Close it in Steam, then retry.'
            }
        }
        $deadline = (Get-Date).AddSeconds(45)
        while (Get-Process VRChat -ErrorAction SilentlyContinue) {
            if ((Get-Date) -ge $deadline) {
                throw 'VRChat is still running. Close it normally in Steam, then retry.'
            }
            Start-Sleep -Milliseconds 500
        }
        $gameClosed = $true
    }

    if (-not (Get-Process vrserver -ErrorAction SilentlyContinue)) {
        Write-Output 'Starting SteamVR...'
        Start-SteamApp 250820
        if (-not (Wait-ForProcess 'vrserver' 60)) {
            throw 'SteamVR did not start within 60 seconds.'
        }
    }

    $setup = $null
    for ($attempt = 0; $attempt -lt 3; $attempt++) {
        $setup = & (Join-Path $bundle 'Test-Setup.ps1') | ConvertFrom-Json
        if ($setup.readyForProviderLaunch) { break }
        $transient = @($setup.reasons | Where-Object {
            $_ -in @('Start SteamVR with a connected HMD.',
                     'Could not read and validate the connected HMD geometry.')
        })
        if ($attempt -eq 2 -or $transient.Count -ne @($setup.reasons).Count) { break }
        Start-Sleep -Seconds 3
    }
    if (-not $setup.readyForProviderLaunch) {
        $reasons = if ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ja') {
            @($setup.reasonsJapanese)
        } else { @($setup.reasons) }
        throw ('Setup is not ready: ' + ($reasons -join '; '))
    }

    Write-Output ('Starting common gaze provider from ' + $InputRoute + '...')
    & (Join-Path $bundle 'Start-Provider.ps1') -InputRoute $InputRoute
    if (-not (Get-Process common_vrss_provider -ErrorAction SilentlyContinue)) {
        throw 'The common gaze provider did not stay running.'
    }
    $providerStarted = $true

    if ($setup.hmdIdentityCandidate) {
        Write-Output 'Launching protected VRChat with the candidate HMD identity...'
        & (Join-Path $bundle 'Start-VRChat-Candidate.ps1') | Out-Null
    } else {
        Write-Output 'Launching VRChat through Steam...'
        Start-SteamApp 438100
    }
    if (-not (Wait-ForProcess 'VRChat' 90)) {
        throw 'VRChat did not start within 90 seconds.'
    }
    $gameLaunched = $true
    Write-Output 'Common gaze provider and VRChat are running. Verify live gaze and VRSS in the headset.'
} catch {
    if ($providerStarted -and -not $gameLaunched) {
        try { & (Join-Path $bundle 'Stop-Provider.ps1') | Out-Null } catch { }
    }
    if ($gameClosed -and -not (Get-Process VRChat -ErrorAction SilentlyContinue)) {
        try {
            Write-Output 'Restoring VRChat after the failed start...'
            Start-SteamApp 438100
        } catch { }
    }
    throw
}
