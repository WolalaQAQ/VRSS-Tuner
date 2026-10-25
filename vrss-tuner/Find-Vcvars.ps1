function Find-Vcvars64 {
    if ($env:VCVARS64_BAT -and (Test-Path -LiteralPath $env:VCVARS64_BAT -PathType Leaf)) {
        return $env:VCVARS64_BAT
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installation) {
            $candidate = Join-Path ($installation | Select-Object -First 1) 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }
    throw 'Visual Studio C++ build tools not found. Install Desktop development with C++ or set VCVARS64_BAT.'
}
