$ErrorActionPreference = 'Stop'
$fixture = Join-Path $PSScriptRoot ('build\common-session-smoke-' + (Get-Date -Format 'yyyyMMddTHHmmssfff') + '-' + $PID)
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'portable\Start-Common-Session.ps1') -Destination $fixture
[IO.File]::WriteAllText((Join-Path $fixture 'Test-Setup.ps1'), @'
$global:VrssSessionEvents.Add('preflight')
[pscustomobject]@{readyForProviderLaunch=$global:VrssSessionReady;hmdIdentityCandidate=$global:VrssSessionCandidate;
    reasons=@('This VRChat profile does not have the VRSS allow gate enabled.');
    reasonsJapanese=@('VRSS is not allowed.')} | ConvertTo-Json -Compress
'@, [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $fixture 'Start-Provider.ps1'), @'
param([string]$InputRoute)
if ($InputRoute -ne 'SRanibro') { throw 'Unexpected input route' }
$global:VrssSessionEvents.Add('provider')
if ($global:VrssSessionProviderFails) { throw 'Provider fixture failure' }
$global:VrssSessionProvider = $true
'Provider started'
'@, [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $fixture 'Start-VRChat-Candidate.ps1'), @'
$global:VrssSessionEvents.Add('candidate-game')
$global:VrssSessionGame = $true
'{}'
'@, [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $fixture 'Stop-Provider.ps1'), @'
$global:VrssSessionEvents.Add('stop-provider')
$global:VrssSessionProvider = $false
'@, [Text.Encoding]::ASCII)

$fakeGame = [pscustomobject]@{}
$fakeGame | Add-Member -MemberType ScriptMethod -Name CloseMainWindow -Value {
    $global:VrssSessionEvents.Add('close-game')
    $global:VrssSessionGame = $false
    return $true
}
function Get-Process {
    [CmdletBinding()]
    param([string]$Name)
    if ($Name -eq 'VRChat' -and $global:VrssSessionGame) { return $global:VrssSessionFakeGame }
    if ($Name -eq 'vrserver' -and $global:VrssSessionSteamVr) { return [pscustomobject]@{Id=1} }
    if ($Name -eq 'common_vrss_provider' -and $global:VrssSessionProvider) { return [pscustomobject]@{Id=2} }
}
function Start-Process {
    [CmdletBinding()]
    param([string]$FilePath)
    if ($FilePath -eq 'steam://rungameid/250820') {
        $global:VrssSessionEvents.Add('start-steamvr')
        $global:VrssSessionSteamVr = $true
    } elseif ($FilePath -eq 'steam://rungameid/438100') {
        $global:VrssSessionEvents.Add('steam-game')
        $global:VrssSessionGame = $true
    } else { throw "Unexpected launch: $FilePath" }
}
function Start-Sleep { param([int]$Milliseconds, [int]$Seconds) }

$global:VrssSessionFakeGame = $fakeGame
try {
    foreach ($case in @(
        @{game=$true;steamvr=$false;candidate=$false;expected='close-game,start-steamvr,preflight,provider,steam-game'},
        @{game=$false;steamvr=$true;candidate=$true;expected='preflight,provider,candidate-game'})) {
        $global:VrssSessionEvents = [Collections.Generic.List[string]]::new()
        $global:VrssSessionGame = $case.game
        $global:VrssSessionSteamVr = $case.steamvr
        $global:VrssSessionProvider = $false
        $global:VrssSessionCandidate = $case.candidate
        $global:VrssSessionReady = $true
        $global:VrssSessionProviderFails = $false
        $output = & (Join-Path $fixture 'Start-Common-Session.ps1') -InputRoute SRanibro
        $actual = $global:VrssSessionEvents -join ','
        if ($actual -ne $case.expected -or -not $global:VrssSessionGame -or
            -not $global:VrssSessionProvider -or
            -not (@($output) -match 'Common gaze provider and VRChat are running')) {
            throw "Common session order failed: $actual (expected $($case.expected))"
        }
    }
    foreach ($failure in @('preflight','provider')) {
        $global:VrssSessionEvents = [Collections.Generic.List[string]]::new()
        $global:VrssSessionGame = $true
        $global:VrssSessionSteamVr = $true
        $global:VrssSessionProvider = $false
        $global:VrssSessionCandidate = $false
        $global:VrssSessionReady = $failure -ne 'preflight'
        $global:VrssSessionProviderFails = $failure -eq 'provider'
        $failed = $false
        try { & (Join-Path $fixture 'Start-Common-Session.ps1') -InputRoute SRanibro | Out-Null }
        catch { $failed = $true }
        if (-not $failed -or -not $global:VrssSessionGame -or $global:VrssSessionProvider -or
            ($global:VrssSessionEvents -join ',') -ne
            $(if ($failure -eq 'preflight') { 'close-game,preflight,steam-game' }
              else { 'close-game,preflight,provider,steam-game' })) {
            throw "Common session failed to restore VRChat after $failure failure."
        }
    }
    Write-Output 'COMMON_SESSION_PASS auto_close steamvr preflight provider steam_launch candidate_launch failure_restore no_UAC'
} finally {
    foreach ($name in @('VrssSessionFakeGame','VrssSessionEvents','VrssSessionGame',
                       'VrssSessionSteamVr','VrssSessionProvider','VrssSessionCandidate',
                       'VrssSessionReady','VrssSessionProviderFails')) {
        Remove-Variable -Name $name -Scope Global -ErrorAction SilentlyContinue
    }
}
