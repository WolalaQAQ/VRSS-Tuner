@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Common-Session.ps1" -InputRoute PSVR2Toolkit
pause
