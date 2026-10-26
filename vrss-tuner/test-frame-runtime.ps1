$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Find-Vcvars.ps1')
$vcvars = Find-Vcvars64
$build = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$testExe = Join-Path $build 'frame_runtime_test.exe'
$line = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 "' +
    (Join-Path $PSScriptRoot 'frame_runtime_test.cpp') + '" /Fo"' +
    (Join-Path $build 'frame_runtime_test.obj') + '" /Fe"' + $testExe + '"'
& cmd.exe /d /c $line
if ($LASTEXITCODE -ne 0) { throw 'Frame runtime test compilation failed' }
& $testExe
if ($LASTEXITCODE -ne 0) { throw 'Frame runtime tests failed' }
