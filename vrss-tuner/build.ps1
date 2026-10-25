param([string]$OutputPath = '', [switch]$StaticRuntime)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$research = Split-Path -Parent $here
$nvapi = Join-Path $research 'third_party\nvapi'
. (Join-Path $here 'Find-Vcvars.ps1')
$vcvars = Find-Vcvars64
$output = if ($OutputPath) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath) } else { Join-Path $here 'VRSS Tuner.exe' }
$build = Join-Path $here 'build'

if (-not (Test-Path -LiteralPath $vcvars)) { throw "Visual Studio C++ tools not found: $vcvars" }
if (-not (Test-Path -LiteralPath (Join-Path $nvapi 'amd64\nvapi64.lib'))) { throw 'NVAPI library not found' }
New-Item -ItemType Directory -Path $build -Force | Out-Null

$runtime = if ($StaticRuntime) { '/MT' } else { '/MD' }
$line = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 ' + $runtime + ' /DUNICODE /D_UNICODE /I"' + $nvapi + '" /Fo"' + (Join-Path $build 'VrssTuner.obj') + '" /Fd"' + (Join-Path $build 'VrssTuner.pdb') + '" "' + (Join-Path $here 'VrssTuner.cpp') + '" /link /SUBSYSTEM:WINDOWS /LIBPATH:"' + (Join-Path $nvapi 'amd64') + '" /OUT:"' + $output + '" /PDB:"' + (Join-Path $build 'VRSS Tuner.pdb') + '"'
& cmd.exe /d /c $line
if ($LASTEXITCODE -ne 0) { throw "C++ build failed with code $LASTEXITCODE" }
Get-Item -LiteralPath $output | Select-Object FullName, Length, LastWriteTime
