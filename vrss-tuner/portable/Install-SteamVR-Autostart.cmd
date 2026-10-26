@echo off
"%~dp0steamvr_autostart.exe" --install
if errorlevel 1 (
  echo SteamVR registration failed. Start SteamVR and retry. If the application was just added, restart SteamVR and retry once.
) else (
  echo VRSS Tuner will start with SteamVR. VRChat remains under your control.
)
pause
