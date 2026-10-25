# Setup and verification (English)

[日本語](SETUP.ja.md) · [Project overview](../README.md)

## Tested configuration and limits

- Headset confirmed with the developer's live visual check: **Pimax Dream Air**.
- GPU/driver in that setup: GeForce RTX 5090, NVIDIA driver 610.88. The provider checks the exact hashes of the audited `nvapi64_impl.dll` and `nvwgf2umx.dll`, not just the displayed driver version. Other driver builds are refused.
- SteamVR/OpenVR headset geometry, a readable NVIDIA VRChat profile with the VRSS allow setting, binocular gaze from Baballonia OSC, and MSAA enabled inside VRChat are required for this route.
- Other headsets, PCs, driver builds, and OSC sources have not been verified. The setup checker can identify prerequisites but cannot prove a visible, moving foveated region.

## Install

1. Download the portable ZIP from [Releases](https://github.com/challenger0303/VRSS-Tuner/releases). Extract the entire ZIP to a writable directory; keep `VRSS Tuner.exe`, scripts and `common-vrss-provider` together. Do not copy only the EXE.
2. Connect the headset and start SteamVR. Leave VRChat closed. Configure Baballonia to send left- and right-eye OSC gaze data to `127.0.0.1:9020`.
3. Run `Test-Setup.cmd`. It reads package hashes, NVIDIA driver, profile settings, SteamVR/HMD geometry and running provider processes. It does not change driver settings. A `readyForProviderLaunch: false` result includes English and Japanese reasons.
4. If the only missing setting is `vrssOn`, open `VRSS Tuner.exe`, choose **On** in Graphics, save and approve the Windows administrator prompt. Run the setup check again. If `driverSupported` or `vrssAllowed` is false, this package cannot activate the common provider on that setup; do not bypass the check.
5. Run `Start-Provider.cmd` and approve the administrator prompt. In the tuner, select the common provider and start gaze input. The provider listens for the tuner's binocular relay on loopback UDP port 5556. The tuner receives Baballonia OSC on loopback port 9020 by default.
6. Launch VRChat and enable MSAA in its graphics settings. Verify in the headset that a high-quality region appears in both eyes and follows gaze horizontally and vertically. A process log alone is not visual confirmation.
7. Finish with `Stop-Provider.cmd`. This requests a graceful stop; it does not terminate SteamVR, VRChat, or a vendor provider.

## Live status in the tuner

The Gaze Input tab separately shows OSC reception, relay transmission, common-provider registration, and the result of gaze updates sent to NVIDIA. The left/right dots show **submitted gaze positions**, not the boundary of the high-quality region. Green NVIDIA gaze means a VR consumer was present and the gaze update API returned success. Verify actual VRSS rendering and alignment in the headset. The Pimax official path exposes no equivalent telemetry to the tuner, so only its process state is shown.

## PSVR2 input route (unverified)

PSVR2 is a headset/input source, not a calibration target. The proposed route is **PSVR2Toolkit → Baballonia OSC → Tuner → common provider → NVIDIA**. Choose **Common provider** as the calibration target, not the Pimax path. Check in the tuner's live view that left/right `LeftEyeX/LeftEyeY/RightEyeX/RightEyeY` values reach port 9020, then calibrate the Y sign, gain, and per-eye position on the actual PSVR2. The common route treats UDP coordinates as calibrated; OpenVR projection geometry does not automatically correct gaze placement. PSVR2 gaze input, provider registration, and a moving VRChat region have **not yet been verified**. Sony's official PC connection does not expose eye tracking; if using the unofficial [PSVR2Toolkit installation guide](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation), review its prerequisites and restrictions first. This package does not include Toolkit binaries or code.

## Position calibration

The bundled `common-vrss-provider/calibration.txt` starts with zero per-eye offsets and horizontal/vertical gain 1. Adjust these using the tuner's gaze controls while watching a suitable high-contrast scene in the headset. Keep your head as still as practical when comparing gaze movement and the rendered region. The initial values are generic and are **not** a Dream Air calibration preset.

## Troubleshooting

| Setup result or symptom | What to check |
| --- | --- |
| `driverSupported: false` | The installed NVIDIA binaries differ from the audited 610.88 build. This release intentionally refuses private ABI calls on unknown binaries. |
| `vrssAllowed: false` | The NVIDIA VRChat profile lacks the VRSS allow setting. This package does not fabricate or replace that profile. |
| `steamVrRunning: false` or `hmdGeometryValid: false` | Connect the HMD, start SteamVR, then rerun the check. |
| Official provider is running | Use that route, or stop it through the vendor's normal controls before using the common provider. The script never stops it automatically. |
| Provider runs but region does not track | Confirm Baballonia sends fresh binocular gaze, the tuner relay is started, VRChat MSAA is enabled, and both eyes are visually checked. |

Diagnostic output and run logs are written under `%LOCALAPPDATA%\VRSS Tuner`. They may contain machine and headset information; inspect before sharing. The package does not include NVIDIA or Pimax drivers and makes no guarantee for unsupported configurations.
