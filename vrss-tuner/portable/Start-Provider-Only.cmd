@echo off
set "route=%~1"
if "%route%"=="" set "route=Baballonia"
if /i "%~2"=="wait" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Common-Session.ps1" -InputRoute "%route%" -ProviderOnly -WaitForHeadset
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Common-Session.ps1" -InputRoute "%route%" -ProviderOnly
  pause
)
