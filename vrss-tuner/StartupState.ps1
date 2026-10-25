function Get-VrssTunerStartupState {
    param([Parameter(Mandatory=$true)][string]$ResearchRoot)
    $root = Join-Path $ResearchRoot 'vrss-tuner'
    $path = Join-Path $root 'startup-state.ini'
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    $fields = @{}
    foreach ($line in [IO.File]::ReadAllLines($path)) {
        if ($line -notmatch '^([a-z0-9_]+)=(.+)$' -or $fields.ContainsKey($matches[1])) { throw 'Invalid VRSS Tuner startup state.' }
        $fields[$matches[1]] = $matches[2]
    }
    $keys = @('format','mode','area','layout','calibration_sha256')
    if ($fields.Count -ne $keys.Count) { throw 'Unexpected VRSS Tuner startup fields.' }
    foreach ($key in $keys) { if (-not $fields.ContainsKey($key)) { throw "Missing VRSS Tuner field: $key" } }
    if ($fields.format -ne '1' -or $fields.mode -notmatch '^[0-3]$' -or
        $fields.area -notmatch '^\d{1,10}$' -or $fields.layout -notmatch '^\d{1,10}$' -or
        $fields.calibration_sha256 -notmatch '^[a-f0-9]{64}$') { throw 'Invalid VRSS Tuner startup values.' }
    $area = [uint32]::Parse($fields.area, [Globalization.CultureInfo]::InvariantCulture)
    $layout = [uint32]::Parse($fields.layout, [Globalization.CultureInfo]::InvariantCulture)
    $areaFloat = [BitConverter]::ToSingle([BitConverter]::GetBytes($area), 0)
    if ([single]::IsNaN($areaFloat) -or [single]::IsInfinity($areaFloat) -or $areaFloat -lt 1 -or $areaFloat -gt 100) {
        throw 'Invalid VRSS Tuner area.'
    }
    $snapshot = Join-Path $root ('startup-pimax-calibration-' + $fields.calibration_sha256 + '.txt')
    if ((Get-Item -LiteralPath $snapshot).Length -gt 256 -or
        (Get-FileHash -LiteralPath $snapshot).Hash -ne $fields.calibration_sha256) { throw 'VRSS Tuner calibration snapshot hash mismatch.' }
    $lines = [IO.File]::ReadAllLines($snapshot)
    if ($lines.Count -ne 2 -and $lines.Count -ne 4) { throw 'Invalid calibration snapshot.' }
    $calibration = @{ left_deg=0.0; right_deg=0.0; x_gain=1.0; y_gain=1.0 }
    $calibrationKeys = @('left_deg','right_deg','x_gain','y_gain')
    for ($i=0; $i -lt $lines.Count; $i++) {
        $prefix = $calibrationKeys[$i] + '='
        if (-not $lines[$i].StartsWith($prefix, [StringComparison]::Ordinal)) { throw 'Invalid calibration field order.' }
        $value = [double]::Parse($lines[$i].Substring($prefix.Length), [Globalization.CultureInfo]::InvariantCulture)
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) { throw 'Nonfinite calibration value.' }
        $calibration[$calibrationKeys[$i]] = $value
    }
    if ([Math]::Abs($calibration.left_deg) -gt 5 -or [Math]::Abs($calibration.right_deg) -gt 5 -or
        $calibration.x_gain -lt 0.1 -or $calibration.x_gain -gt 2 -or
        [Math]::Abs($calibration.y_gain) -lt 0.1 -or [Math]::Abs($calibration.y_gain) -gt 2) { throw 'Out-of-range calibration snapshot.' }
    [pscustomobject]@{
        source='VRSS Tuner'; snapshot=$snapshot; calibrationSha256=$fields.calibration_sha256
        nvidia=@{ autovrs_settings=[uint32]$fields.mode; fovea_area_percent=$area; vr_only_mode=$layout }
    }
}
