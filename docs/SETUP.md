# Setup and verification (English)

[日本語](SETUP.ja.md) · [Project overview](../README.md)

This guide covers the native Pimax and common-provider routes. Steam Frame on NVIDIA 596.02 uses the separate [Frame guide](STEAM-FRAME.md).

## Tested configuration and limits

- Headset confirmed with the developer's live visual check: **Pimax Dream Air**.
- GPU/driver in that setup: GeForce RTX 5090, NVIDIA driver 610.88. The provider checks the exact hashes and gaze-handler methods of the audited `nvapi64_impl.dll` and `nvwgf2umx.dll`, not just the displayed driver version. An explicit experimental contract for another exact image pair is described in [driver contracts](DRIVER-CONTRACTS.md); unmatched builds are refused.
- The common route needs SteamVR/OpenVR headset geometry, an NVIDIA VRChat profile that allows VRSS, valid gaze from the selected source, and MSAA enabled inside VRChat. The native Pimax route uses Pimax eye tracking.
- Other headsets, PCs and driver builds are hardware-unverified. The setup checker can identify prerequisites but cannot prove a visible, moving high-quality region.

## Install

1. Download the Windows installer from [Releases](https://github.com/challenger0303/VRSS-Tuner/releases), or extract the entire portable ZIP for manual testing. The installer places all files in a fixed per-user location. Open the tuner and choose a language at the top right.
2. **Native Pimax XR5:** Enable headset eye tracking, select **Pimax path (native priority)** in Gaze input, turn VRSS **On** in Graphics and apply it. Start SteamVR and VRChat normally. Do not run the common-provider scripts.
3. **Common provider with SRanibro, PSVR2 Toolkit or another source:** Start the headset and its gaze software, then connect SteamVR. Select one source in Gaze input, turn VRSS **On** in Graphics and apply it, then click **Start gaze path only**. This runs setup checks and starts the provider without touching VRChat. Approve the Windows administrator prompt manually. You can launch VRChat yourself afterward.
4. **Steam Frame:** Follow the [Frame guide](STEAM-FRAME.md) instead of the common launch button.
5. Enable MSAA in VRChat's Graphics settings. Start with x4 for comparison. Verify in the headset that a high-quality region appears in both eyes and follows gaze horizontally and vertically. A green status in the tuner alone does not prove the rendered effect.
6. When finished with the common route, run `Stop-Provider.cmd`. This leaves VRChat, SteamVR and vendor providers running. The native Pimax route does not need this script.

For a manual, read-only common-route check, run `Test-Setup.cmd`. If `driverSupported` or `vrssAllowed` is false, the common provider cannot start on that setup. `Start-Provider.cmd` is for provider-only diagnostics. Baballonia sends binocular OSC to `127.0.0.1:9020` by default.

For a later launch with a fixed HMD key, start the provider and run `Start-VRChat-HMD-Key.cmd`. To use Steam's normal Play button, run `Install-Steam-Launch-Bridge.cmd` once and paste its launch option into Steam > VRChat > Properties > General. The bridge checks the current HMD at launch and reserves a validated fixed session ID even if the selected common provider is still starting. The HMD ID cannot be changed inside a running VRChat process, but the provider was stopped and reconnected to the same protected VRChat process on the developer's PC.

For SteamVR autostart, use the installer. It installs to a fixed per-user Programs location and registers only when SteamVR is already running. If registration is pending, start SteamVR normally from Steam and run `Install-SteamVR-Autostart.cmd` from the installed folder once. Setup never launches SteamVR. On later starts, SteamVR opens the tuner and attempts to start the selected common gaze path; a Windows administrator prompt may still need manual confirmation. The new installer registration succeeded on the developer's PC, but a fresh SteamVR start after installing this candidate is not yet verified.

## Live status in the tuner

The Gaze Input tab shows the source, tuner relay, provider registration and NVIDIA gaze updates separately. Gray means stopped or awaiting data; amber means more checks are needed; green means that stage was observed. The left/right dots show **submitted gaze positions**, not the boundary of the high-quality region. Verify actual rendering and alignment in the headset. Available telemetry differs on the native Pimax route.

## Pimax path

**Pimax path (native priority)** uses the native Pimax XR5 runtime while it is available. If SRanibro takes over the camera, a separately installed Pimax eye-device bridge registers only when it receives fresh binocular gaze. The tuner does not start SRanibro or prevent a return to the native path. The tested bridge binary is included under `pimax-eye-bridge/` in the portable ZIP, but it is not installed automatically.

On the tested PC, SRanibro 0.1.10-beta in `pimax_xr5` mode stopped and disabled the native service. If the service does not return after SRanibro exits, click **Restore native service** in the tuner and approve Windows elevation. The button is unavailable while SRanibro is running. The Pimax eye device may still need to be re-registered; if gaze does not return, use **Restart service** in the Pimax app (this may close VR apps). A running service or provider does not prove valid gaze; verify that the high-quality region follows eye movement in the headset.

## PSVR2 input route (unverified)

Two PSVR2 input paths are available. **PSVR2 Toolkit (direct)** reads the installed Toolkit CAPI and sends binocular gaze through the common provider. Select it in Tuner, then run `Start-PSVR2-Toolkit.cmd`. The bridge requires a Toolkit build exposing `psvr2_toolkit_capi.dll`; Toolkit itself is not bundled. **SRanibro (common)** reads the local BrokenEye-format binocular stream on port 5555. Select it in Tuner, start SRanibro, and run `Start-SRanibro.cmd`. This SRanibro path is headset-independent: it uses the connected HMD's OpenVR projection geometry, but each HMD still needs gaze mapping validation and calibration. Do not run both sources together. The Gaze Calibration tab uses the common calibration file; save headset-specific values in named profiles. PSVR2 gaze reception, NVIDIA registration, and a moving VRChat high-quality region remain **unverified on hardware**. Check the [PSVR2 Toolkit installation guide](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation) for its own version requirements.

## VIVE input routes (hardware unverified)

For **VIVE Pro Eye**, install and calibrate the official SRanipal runtime, choose the Pro Eye source in Tuner, and run `Start-Vive-Pro-Eye.cmd`. For **VIVE Streaming**, enable PC eye-data forwarding in the vendor streamer, choose its source and run `Start-Vive-Streaming.cmd`. Set `VRSS_VIVE_SDK_DLL` to the full path of the matching installed vendor DLL when automatic discovery fails. Neither vendor DLL is bundled. Both bridges were built and passed synthetic mapping tests, but actual gaze reception and VRChat VRSS2 rendering on VIVE hardware are unverified. See the [HMD compatibility audit](HMD-GAZE-COMPATIBILITY.md).

## Position calibration

The bundled `common-vrss-provider/calibration.txt` starts with zero per-eye offsets and horizontal/vertical gain 1. Adjust these using the tuner's gaze controls while watching a suitable high-contrast scene in the headset. Keep your head as still as practical when comparing gaze movement and the rendered region. The initial values are generic and are **not** a Dream Air calibration preset.

## Troubleshooting

| Setup result or symptom | What to check |
| --- | --- |
| `driverSupported: false` | The installed NVIDIA binaries or gaze-handler methods do not match a built-in or explicitly selected experimental contract. Use `Audit-Driver.cmd` to collect a read-only report; do not assume a matching marketing version is sufficient. |
| `vrssAllowed: false` | The NVIDIA VRChat profile lacks the VRSS allow setting. This package does not fabricate or replace that profile. |
| `steamVrRunning: false` or `hmdGeometryValid: false` | Connect the HMD, start SteamVR, then rerun the check. |
| Official provider is running | Use that route, or stop it through the vendor's normal controls before using the common provider. The script never stops it automatically. |
| Provider runs but region does not track | Check fresh binocular gaze from the selected source, each tuner input/provider/NVIDIA stage, VRChat MSAA and both eyes in the headset. Baballonia also requires starting the tuner relay. |

Diagnostic output and run logs are written under `%LOCALAPPDATA%\VRSS Tuner`. They may contain machine and headset information; inspect before sharing. The package does not include NVIDIA or Pimax drivers and makes no guarantee for unsupported configurations.
