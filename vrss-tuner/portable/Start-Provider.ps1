param([switch]$Worker,[string]$RunDirectory)
$ErrorActionPreference = 'Stop'
$bundle = $PSScriptRoot
$data = Join-Path $env:LOCALAPPDATA 'VRSS Tuner'
$runs = Join-Path $data 'runs'
if ($Worker) {
    $resolved = [IO.Path]::GetFullPath($RunDirectory)
    $allowed = [IO.Path]::GetFullPath($runs).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $resolved -PathType Container)) { throw 'Invalid run directory' }
    $provider = Join-Path $bundle 'common-vrss-provider\common_vrss_provider.exe'
    $geometry = Join-Path $resolved 'geometry.txt'
    $calibration = Join-Path $bundle 'common-vrss-provider\calibration.txt'
    $manifest = Get-Content -LiteralPath (Join-Path $bundle 'bundle-manifest.json') -Raw | ConvertFrom-Json
    $expected = @($manifest.files | Where-Object { $_.path -eq 'common-vrss-provider/common_vrss_provider.exe' })
    if ($expected.Count -ne 1 -or (Get-FileHash -LiteralPath $provider).Hash -ne $expected[0].sha256)
        { throw 'Provider file changed after preflight' }
    try {
        & $provider --geometry $geometry --config $calibration --output $resolved --source udp --port 5556 --seconds 3600 `
            1> (Join-Path $resolved 'provider.stdout.txt') 2> (Join-Path $resolved 'provider.stderr.txt')
        $LASTEXITCODE | Set-Content -LiteralPath (Join-Path $resolved 'exit-code.txt') -Encoding ASCII
    } catch {
        $_.Exception.Message | Set-Content -LiteralPath (Join-Path $resolved 'worker-error.txt') -Encoding UTF8
    }
    return
}
$status = & (Join-Path $bundle 'Test-Setup.ps1') | ConvertFrom-Json
if (-not $status.readyForProviderLaunch) {
    $messages = if ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ja')
        { $status.reasonsJapanese } else { $status.reasons }
    $messages | ForEach-Object { Write-Output "BLOCKED: $_" }
    exit 2
}
New-Item -ItemType Directory -Path $runs -Force | Out-Null
$run = Join-Path $runs ((Get-Date -Format 'yyyyMMddTHHmmssfff') + '-' + $PID)
New-Item -ItemType Directory -Path $run -ErrorAction Stop | Out-Null
Copy-Item -LiteralPath (Join-Path $data 'preflight\geometry.txt') -Destination (Join-Path $run 'geometry.txt')
$powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$argsText = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Worker -RunDirectory "' + $run + '"'
$worker = Start-Process -FilePath $powershell -ArgumentList $argsText -Verb RunAs -WindowStyle Hidden -PassThru
$run | Set-Content -LiteralPath (Join-Path $data 'active-run.txt') -Encoding UTF8
$deadline = (Get-Date).AddSeconds(8)
do {
    if (Get-Process common_vrss_provider -ErrorAction SilentlyContinue) {
        Write-Output "Provider started. Run directory: $run"
        if ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ja')
            { Write-Output 'Tunerの視線入力を開始してから、MSAAを有効にしたVRChatを起動してください。' }
        else { Write-Output 'Now start the gaze relay in VRSS Tuner, then launch VRChat with MSAA enabled.' }
        return
    }
    if (Test-Path -LiteralPath (Join-Path $run 'worker-error.txt')) { throw (Get-Content -LiteralPath (Join-Path $run 'worker-error.txt') -Raw) }
    if (Test-Path -LiteralPath (Join-Path $run 'exit-code.txt')) { throw "Provider exited: $(Get-Content -LiteralPath (Join-Path $run 'exit-code.txt') -Raw)" }
    Start-Sleep -Milliseconds 200
} while ((Get-Date) -lt $deadline)
throw "Provider startup not confirmed. Inspect logs in $run"
