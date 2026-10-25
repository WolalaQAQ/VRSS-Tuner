$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$research = Split-Path -Parent $here
. (Join-Path $here 'Find-Vcvars.ps1')
$vcvars = Find-Vcvars64
$nvapi = Join-Path $research 'third_party\nvapi'
$stamp = Get-Date -Format 'yyyyMMddTHHmmssfff'
$release = Join-Path $here 'release'
$bundle = Join-Path $release ('VRSS-Tuner-Portable-' + $stamp)
$common = Join-Path $bundle 'common-vrss-provider'
New-Item -ItemType Directory -Path $common -ErrorAction Stop | Out-Null
& (Join-Path $here 'build.ps1') -OutputPath (Join-Path $bundle 'VRSS Tuner.exe') -StaticRuntime | Out-Null
$providerSource = Join-Path $research 'common-vrss-provider\provider.cpp'
$providerExe = Join-Path $common 'common_vrss_provider.exe'
$line = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 /MT /I"' + $nvapi + '" /Fo"' + (Join-Path $here 'build\provider.portable.obj') + '" "' + $providerSource + '" /link /LIBPATH:"' + (Join-Path $nvapi 'amd64') + '" /OUT:"' + $providerExe + '"'
& cmd.exe /d /c $line
if ($LASTEXITCODE -ne 0) { throw 'Portable provider build failed' }
$openvr = Join-Path $research 'third_party\openvr'
$exportSource = Join-Path $research 'common-vrss-provider\export_openvr_geometry.cpp'
$exportExe = Join-Path $common 'export_openvr_geometry.exe'
$line = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 /MT /I"' + $research + '" /Fo"' + (Join-Path $here 'build\export_openvr_geometry.portable.obj') + '" "' + $exportSource + '" /link "' + (Join-Path $openvr 'openvr_api.lib') + '" /OUT:"' + $exportExe + '"'
& cmd.exe /d /c $line
if ($LASTEXITCODE -ne 0) { throw 'Portable geometry exporter build failed' }
Copy-Item -LiteralPath (Join-Path $openvr 'openvr_api.dll') -Destination (Join-Path $common 'openvr_api.dll')
Copy-Item -LiteralPath (Join-Path $here 'portable\calibration.txt') -Destination (Join-Path $common 'calibration.txt')
foreach ($name in @('Test-Setup.ps1','Start-Provider.ps1','Stop-Provider.ps1','Test-Setup.cmd','Start-Provider.cmd','Stop-Provider.cmd')) {
    Copy-Item -LiteralPath (Join-Path $here "portable\$name") -Destination (Join-Path $bundle $name)
}
Copy-Item -LiteralPath (Join-Path $here 'portable\README.md') -Destination (Join-Path $bundle 'README.md')
Copy-Item -LiteralPath (Join-Path $here 'portable\README.en.md') -Destination (Join-Path $bundle 'README.en.md')
Copy-Item -LiteralPath (Join-Path $openvr 'LICENSE') -Destination (Join-Path $bundle 'OpenVR-LICENSE.txt') -ErrorAction Stop
Copy-Item -LiteralPath (Join-Path $nvapi 'License.txt') -Destination (Join-Path $bundle 'NVAPI-LICENSE.txt')
$files = @('VRSS Tuner.exe','common-vrss-provider/common_vrss_provider.exe',
    'common-vrss-provider/export_openvr_geometry.exe','common-vrss-provider/openvr_api.dll',
    'common-vrss-provider/calibration.txt',
    'Test-Setup.ps1','Start-Provider.ps1','Stop-Provider.ps1',
    'Test-Setup.cmd','Start-Provider.cmd','Stop-Provider.cmd','README.md','README.en.md',
    'OpenVR-LICENSE.txt','NVAPI-LICENSE.txt')
$entries = foreach ($name in $files) {
    [ordered]@{path=$name;sha256=(Get-FileHash -LiteralPath (Join-Path $bundle $name) -Algorithm SHA256).Hash}
}
[ordered]@{format=1;builtAt=(Get-Date).ToString('o');files=@($entries)} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $bundle 'bundle-manifest.json') -Encoding UTF8
$zip = $bundle + '.zip'
Compress-Archive -LiteralPath $bundle -DestinationPath $zip -CompressionLevel Optimal
[pscustomobject]@{Bundle=$bundle;Zip=$zip;Sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash}
