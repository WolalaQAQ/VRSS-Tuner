@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Provider.ps1" -InputRoute Varjo
pause
