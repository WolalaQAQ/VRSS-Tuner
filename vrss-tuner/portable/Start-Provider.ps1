param([ValidateSet('Baballonia','PSVR2Toolkit','SRanibro','ViveProEye','ViveStreaming','SteamVREye','Varjo')][string]$InputRoute='Baballonia',
      [switch]$Worker,[string]$RunDirectory,[switch]$AllowRunningVRChat)
$ErrorActionPreference = 'Stop'
$bundle = $PSScriptRoot
$data = Join-Path $env:LOCALAPPDATA 'VRSS Tuner'
$calibration = Join-Path $data 'common-calibration.txt'
$runs = Join-Path $data 'runs'
function Get-PortableSha256([string]$path) {
    $stream = [IO.File]::OpenRead($path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Find-ViveSdk([string]$name) {
    if ($env:VRSS_VIVE_SDK_DLL) {
        $candidate = [IO.Path]::GetFullPath($env:VRSS_VIVE_SDK_DLL)
        if ((Split-Path -Leaf $candidate) -ne $name -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw "VRSS_VIVE_SDK_DLL must point to an installed $name"
        }
        return $candidate
    }
    $roots = @((Join-Path $env:ProgramFiles 'VIVE\SRanipal'))
    if (${env:ProgramFiles(x86)}) { $roots += (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\VIVEDriver\App\SRanipal') }
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
        $candidate = Get-ChildItem -LiteralPath $root -Filter $name -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($candidate) { return $candidate.FullName }
    }
    throw "$name not found. Set VRSS_VIVE_SDK_DLL to the installed vendor DLL path. No vendor DLL is bundled."
}
if ($Worker) {
    $resolved = [IO.Path]::GetFullPath($RunDirectory)
    $allowed = [IO.Path]::GetFullPath($runs).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $resolved -PathType Container)) { throw 'Invalid run directory' }
    $provider = Join-Path $bundle 'common-vrss-provider\common_vrss_provider.exe'
    $geometry = Join-Path $resolved 'geometry.txt'
    $manifest = Get-Content -LiteralPath (Join-Path $bundle 'bundle-manifest.json') -Raw | ConvertFrom-Json
    $expected = @($manifest.files | Where-Object { $_.path -eq 'common-vrss-provider/common_vrss_provider.exe' })
    if ($expected.Count -ne 1 -or (Get-PortableSha256 $provider) -ne $expected[0].sha256)
        { throw 'Provider file changed after preflight' }
    $source = if ($InputRoute -eq 'SRanibro') { 'tcp' } else { 'udp' }
    $port = if ($InputRoute -eq 'SRanibro') { 5555 } else { 5556 }
    try {
        & $provider --geometry $geometry --config $calibration --output $resolved --source $source --port $port --seconds 3600 `
            1> (Join-Path $resolved 'provider.stdout.txt') 2> (Join-Path $resolved 'provider.stderr.txt')
        $LASTEXITCODE | Set-Content -LiteralPath (Join-Path $resolved 'exit-code.txt') -Encoding ASCII
    } catch {
        $_.Exception.Message | Set-Content -LiteralPath (Join-Path $resolved 'worker-error.txt') -Encoding UTF8
    }
    return
}
$status = & (Join-Path $bundle 'Test-Setup.ps1') -AllowRunningVRChat:$AllowRunningVRChat | ConvertFrom-Json
if (-not $status.readyForProviderLaunch) {
    $messages = if ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ja')
        { $status.reasonsJapanese } else { $status.reasons }
    $messages | ForEach-Object { Write-Output "BLOCKED: $_" }
    exit 2
}
if (Get-Process common_vrss_provider,vrss_gaze_provider -ErrorAction SilentlyContinue) {
    throw 'A VRSS gaze provider is already running. Stop the active route before starting another.'
}
if ($InputRoute -eq 'PSVR2Toolkit') {
    $bridge = Join-Path $bundle 'psvr2-gaze-bridge\psvr2_gaze_bridge.exe'
    $check = & $bridge --check-exports-only 1 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or -not $check.Contains('PSVR2TK_CAPI_EXPORTS_READY')) {
        throw "PSVR2 Toolkit CAPI is unavailable: $check"
    }
}
if ($InputRoute -eq 'SRanibro' -and -not (Get-Process SRanibro -ErrorAction SilentlyContinue)) {
    throw 'SRanibro is not running. Start its binocular gaze stream first.'
}
$viveSdk = $null
if ($InputRoute -in @('ViveProEye','ViveStreaming')) {
    $dll = if ($InputRoute -eq 'ViveProEye') { 'SRanipal.dll' } else { 'VS_PC_SDK.dll' }
    $viveSdk = Find-ViveSdk $dll
    $viveBridge = Join-Path $bundle 'vive-streaming-gaze-bridge\ViveStreamingGazeBridge.exe'
    if (-not (Test-Path -LiteralPath $viveBridge -PathType Leaf)) { throw 'VIVE gaze bridge is missing from this package.' }
}
if ($InputRoute -eq 'SteamVREye') {
    $steamBridge = Join-Path $bundle 'steamvr-gaze-bridge\steamvr_gaze_bridge.exe'
    $steamManifest = Join-Path $bundle 'steamvr-gaze-bridge\eye_actions.json'
    if (-not (Test-Path -LiteralPath $steamBridge -PathType Leaf) -or
        -not (Test-Path -LiteralPath $steamManifest -PathType Leaf)) {
        throw 'SteamVR eye-action bridge is missing from this package.'
    }
}
if ($InputRoute -eq 'Varjo') {
    $varjoBridge = Join-Path $bundle 'varjo-gaze-bridge\varjo_gaze_bridge.exe'
    if (-not (Test-Path -LiteralPath $varjoBridge -PathType Leaf)) {
        throw 'Varjo gaze bridge is missing from this package.'
    }
    if ($env:VRSS_VARJO_DLL) {
        $varjoDll = [IO.Path]::GetFullPath($env:VRSS_VARJO_DLL)
        if ((Split-Path -Leaf $varjoDll) -ne 'VarjoLib.dll' -or
            -not (Test-Path -LiteralPath $varjoDll -PathType Leaf)) {
            throw 'VRSS_VARJO_DLL must point to an installed VarjoLib.dll.'
        }
    } else {
        $varjoDll = Join-Path $env:ProgramFiles 'Varjo\varjo-compositor\VarjoLib.dll'
        if (-not (Test-Path -LiteralPath $varjoDll -PathType Leaf)) {
            throw 'VarjoLib.dll not found. Set VRSS_VARJO_DLL to the installed DLL path.'
        }
    }
}
New-Item -ItemType Directory -Path $runs -Force | Out-Null
$run = Join-Path $runs ((Get-Date -Format 'yyyyMMddTHHmmssfff') + '-' + $PID)
New-Item -ItemType Directory -Path $run -ErrorAction Stop | Out-Null
Copy-Item -LiteralPath (Join-Path $data 'preflight\geometry.txt') -Destination (Join-Path $run 'geometry.txt')
$InputRoute | Set-Content -LiteralPath (Join-Path $run 'input-route.txt') -Encoding ASCII
$powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$argsText = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Worker -RunDirectory "' + $run + '" -InputRoute ' + $InputRoute
$workerProcess = Start-Process -FilePath $powershell -ArgumentList $argsText -Verb RunAs -WindowStyle Hidden -PassThru
if (-not $workerProcess) { throw 'Could not start the elevated provider worker.' }
$run | Set-Content -LiteralPath (Join-Path $data 'active-run.txt') -Encoding UTF8
$deadline = (Get-Date).AddSeconds(8)
do {
    if (Get-Process common_vrss_provider -ErrorAction SilentlyContinue) {
        if ($InputRoute -eq 'PSVR2Toolkit') {
            $bridge = Join-Path $bundle 'psvr2-gaze-bridge\psvr2_gaze_bridge.exe'
            $bridgeArgs = '--geometry "' + (Join-Path $run 'geometry.txt') + '" --config "' + $calibration +
                '" --stop-file "' + (Join-Path $run 'stop.request') + '" --port 5556 --seconds 3600'
            $gaze = Start-Process -FilePath $bridge -ArgumentList $bridgeArgs -WindowStyle Hidden -PassThru
            $gaze.Id | Set-Content -LiteralPath (Join-Path $run 'psvr2-bridge.pid') -Encoding ASCII
        } elseif ($InputRoute -in @('ViveProEye','ViveStreaming')) {
            $source = if ($InputRoute -eq 'ViveProEye') { 'pro-eye' } else { 'vive-streaming' }
            $bridge = Join-Path $bundle 'vive-streaming-gaze-bridge\ViveStreamingGazeBridge.exe'
            $manifest = Get-Content -LiteralPath (Join-Path $bundle 'bundle-manifest.json') -Raw | ConvertFrom-Json
            $expected = @($manifest.files | Where-Object { $_.path -eq 'vive-streaming-gaze-bridge/ViveStreamingGazeBridge.exe' })
            if ($expected.Count -ne 1 -or (Get-PortableSha256 $bridge) -ne $expected[0].sha256) {
                throw 'VIVE gaze bridge file changed after packaging.'
            }
            $bridgeArgs = '--geometry "' + (Join-Path $run 'geometry.txt') + '" --config "' + $calibration +
                '" --stop-file "' + (Join-Path $run 'stop.request') + '" --source ' + $source +
                ' --sdk "' + $viveSdk + '" --port 5556 --seconds 3600'
            $gaze = Start-Process -FilePath $bridge -ArgumentList $bridgeArgs -WindowStyle Hidden -PassThru
            $gaze.Id | Set-Content -LiteralPath (Join-Path $run 'vive-bridge.pid') -Encoding ASCII
        } elseif ($InputRoute -eq 'SteamVREye') {
            $bridge = Join-Path $bundle 'steamvr-gaze-bridge\steamvr_gaze_bridge.exe'
            $bridgeArgs = '--geometry "' + (Join-Path $run 'geometry.txt') + '" --config "' + $calibration +
                '" --manifest "' + (Join-Path $bundle 'steamvr-gaze-bridge\eye_actions.json') +
                '" --stop-file "' + (Join-Path $run 'stop.request') + '" --port 5556 --seconds 3600'
            $gaze = Start-Process -FilePath $bridge -ArgumentList $bridgeArgs -WindowStyle Hidden -PassThru
            $gaze.Id | Set-Content -LiteralPath (Join-Path $run 'steamvr-bridge.pid') -Encoding ASCII
        } elseif ($InputRoute -eq 'Varjo') {
            $bridge = Join-Path $bundle 'varjo-gaze-bridge\varjo_gaze_bridge.exe'
            $env:PATH = (Split-Path -Parent $varjoDll) + ';' + $env:PATH
            $gainValues = @{}
            foreach ($line in Get-Content -LiteralPath $calibration) {
                $parts = $line -split '=',2
                if ($parts.Count -eq 2) { $gainValues[$parts[0]] = $parts[1] }
            }
            $culture = [Globalization.CultureInfo]::InvariantCulture
            $xGain = [double]::Parse($gainValues['x_gain'],$culture).ToString($culture)
            $yGain = [double]::Parse($gainValues['y_gain'],$culture).ToString($culture)
            $bridgeArgs = '--geometry "' + (Join-Path $run 'geometry.txt') +
                '" --x-gain ' + $xGain + ' --y-gain ' + $yGain + ' --port 5556 --seconds 3600'
            $gaze = Start-Process -FilePath $bridge -ArgumentList $bridgeArgs -WindowStyle Hidden -PassThru
            $gaze.Id | Set-Content -LiteralPath (Join-Path $run 'varjo-bridge.pid') -Encoding ASCII
        }
        Write-Output "Provider started. Run directory: $run"
        if ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ja')
            { Write-Output '選択した視線経路をTunerで確認してから、MSAAを有効にしたVRChatを起動してください。' }
        else { Write-Output 'Check the selected gaze route in Tuner, then launch VRChat with MSAA enabled.' }
        return
    }
    if (Test-Path -LiteralPath (Join-Path $run 'worker-error.txt')) { throw (Get-Content -LiteralPath (Join-Path $run 'worker-error.txt') -Raw) }
    if (Test-Path -LiteralPath (Join-Path $run 'exit-code.txt')) { throw "Provider exited: $(Get-Content -LiteralPath (Join-Path $run 'exit-code.txt') -Raw)" }
    Start-Sleep -Milliseconds 200
} while ((Get-Date) -lt $deadline)
throw "Provider startup not confirmed. Inspect logs in $run"
