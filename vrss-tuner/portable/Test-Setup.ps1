param()
$ErrorActionPreference = 'Stop'
$bundle = $PSScriptRoot
$provider = Join-Path $bundle 'common-vrss-provider\common_vrss_provider.exe'
$exporter = Join-Path $bundle 'common-vrss-provider\export_openvr_geometry.exe'
$tuner = Join-Path $bundle 'VRSS Tuner.exe'
$calibration = Join-Path $bundle 'common-vrss-provider\calibration.txt'
$data = Join-Path $env:LOCALAPPDATA 'VRSS Tuner\preflight'
New-Item -ItemType Directory -Path $data -Force | Out-Null
$reasons = [Collections.Generic.List[string]]::new()
$integrity = $true
$manifest = Get-Content -LiteralPath (Join-Path $bundle 'bundle-manifest.json') -Raw | ConvertFrom-Json
$bundleFull = [IO.Path]::GetFullPath($bundle).TrimEnd('\') + '\'
foreach ($entry in $manifest.files) {
    $path = [IO.Path]::GetFullPath((Join-Path $bundle $entry.path))
    if (-not $path.StartsWith($bundleFull, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) {
        $integrity = $false
        $reasons.Add("Package file missing or changed: $($entry.path)")
    }
}
$driver = $false
$driverMessage = 'not checked'
if ($integrity) {
    $driverOutput = & $provider --driver-check-only 1 2>&1 | Out-String
    $driver = $LASTEXITCODE -eq 0 -and $driverOutput.Contains('DRIVER_CHECK_PASS')
    $driverMessage = $driverOutput.Trim()
    if (-not $driver) { $reasons.Add('NVIDIA driver image is not one of the audited versions.') }
}
$details = @{}
if ($integrity) {
    $detailsFile = Join-Path $data 'tuner-readiness.txt'
    $tunerRun = Start-Process -FilePath $tuner -ArgumentList ('--readiness "' + $detailsFile + '"') -WindowStyle Hidden -Wait -PassThru
    if ($tunerRun.ExitCode -eq 0 -and (Test-Path -LiteralPath $detailsFile)) {
        foreach ($line in Get-Content -LiteralPath $detailsFile) {
            $parts = $line -split '=',2
            if ($parts.Count -eq 2) { $details[$parts[0]] = $parts[1] }
        }
    }
}
$profile = $details['nvapi_vrchat_profile'] -eq 'readable'
$allow = $details['vrss_allow'] -eq 'enabled'
$mode = $details['vrss_mode'] -eq 'on'
$config = $details['common_calibration'] -eq 'readable'
if (-not $profile) { $reasons.Add('NVIDIA VRChat profile is not readable.') }
if (-not $allow) { $reasons.Add('This VRChat profile does not have the VRSS allow gate enabled.') }
if (-not $mode) { $reasons.Add('VRSS is Off. Enable it in VRSS Tuner before launching VRChat.') }
if (-not $config) { $reasons.Add('Common-provider calibration is missing or invalid.') }
$steamVr = [bool](Get-Process vrserver -ErrorAction SilentlyContinue)
$vrchat = [bool](Get-Process VRChat -ErrorAction SilentlyContinue)
$official = [bool](Get-Process vrss_gaze_provider -ErrorAction SilentlyContinue)
$common = [bool](Get-Process common_vrss_provider -ErrorAction SilentlyContinue)
if (-not $steamVr) { $reasons.Add('Start SteamVR with a connected HMD.') }
if ($vrchat) { $reasons.Add('Close VRChat normally before starting the gaze provider.') }
if ($official) { $reasons.Add('An official gaze provider is active; keep that route or stop it using the vendor controls.') }
if ($common) { $reasons.Add('A common provider is already running.') }
$geometry = $false
$geometryFile = Join-Path $data 'geometry.txt'
if ($integrity -and $steamVr) {
    $null = & $exporter $geometryFile 2>&1 | Out-String
    $geometry = $LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $geometryFile)
    if ($geometry -and $config) {
        $null = & $provider --geometry $geometryFile --config $calibration --output $data --check-only 1 2>&1 | Out-String
        $geometry = $LASTEXITCODE -eq 0
    }
    if (-not $geometry) { $reasons.Add('Could not read and validate the connected HMD geometry.') }
}
$reasonsJapanese = @(foreach ($reason in $reasons) {
    switch ($reason) {
        'NVIDIA driver image is not one of the audited versions.' { 'NVIDIAドライバーが監査済みの版と一致しません。'; break }
        'NVIDIA VRChat profile is not readable.' { 'NVIDIAのVRChatプロファイルを読み取れません。'; break }
        'This VRChat profile does not have the VRSS allow gate enabled.' { 'このVRChatプロファイルではVRSSが許可されていません。'; break }
        'VRSS is Off. Enable it in VRSS Tuner before launching VRChat.' { 'VRSSがオフです。VRChat起動前にTunerでオンにしてください。'; break }
        'Common-provider calibration is missing or invalid.' { '共通プロバイダーの補正ファイルがありません、または無効です。'; break }
        'Start SteamVR with a connected HMD.' { 'HMDを接続してSteamVRを起動してください。'; break }
        'Close VRChat normally before starting the gaze provider.' { '視線プロバイダーの起動前にVRChatを通常終了してください。'; break }
        'An official gaze provider is active; keep that route or stop it using the vendor controls.' { '公式視線プロバイダーが動作中です。公式経路を使うか、ベンダーの操作で停止してください。'; break }
        'A common provider is already running.' { '共通プロバイダーはすでに動作しています。'; break }
        'Could not read and validate the connected HMD geometry.' { '接続中のHMDの投影情報を読み取り・検証できません。'; break }
        default { "パッケージのファイルが不足または変更されています: $reason" }
    }
})
[pscustomobject]@{
    format = 1
    readyForProviderLaunch = $reasons.Count -eq 0
    packageIntegrity = $integrity
    driverSupported = $driver
    driverMessage = $driverMessage
    vrchatProfileReadable = $profile
    vrssAllowed = $allow
    vrssOn = $mode
    commonCalibrationReadable = $config
    steamVrRunning = $steamVr
    vrchatRunning = $vrchat
    officialProviderRunning = $official
    commonProviderRunning = $common
    hmdGeometryValid = $geometry
    gazeInput = 'Start Baballonia relay from VRSS Tuner after provider launch; live tracking still requires a headset check.'
    reasons = @($reasons)
    reasonsJapanese = @($reasonsJapanese)
} | ConvertTo-Json -Depth 4
