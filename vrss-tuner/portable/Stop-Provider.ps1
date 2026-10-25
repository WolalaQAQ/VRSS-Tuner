$ErrorActionPreference = 'Stop'
$data = Join-Path $env:LOCALAPPDATA 'VRSS Tuner'
$record = Join-Path $data 'active-run.txt'
if (-not (Test-Path -LiteralPath $record)) { throw 'No active run recorded' }
$run = (Get-Content -LiteralPath $record -Raw).Trim()
$allowed = [IO.Path]::GetFullPath((Join-Path $data 'runs')).TrimEnd('\') + '\'
$resolved = [IO.Path]::GetFullPath($run)
if (-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
    -not (Test-Path -LiteralPath $resolved -PathType Container)) { throw 'Invalid run directory' }
'User requested stop' | Set-Content -LiteralPath (Join-Path $resolved 'stop.request') -Encoding ASCII
if ([Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'ja')
    { Write-Output '停止を要求しました。次の更新後にプロバイダーが終了します。' }
else { Write-Output 'Stop requested. The provider will exit after its next update.' }
