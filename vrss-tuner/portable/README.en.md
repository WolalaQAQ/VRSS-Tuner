# VRSS Tuner Portable

This is an experimental Windows app for NVIDIA VRSS2 settings and gaze input. Switch between Japanese and English at the upper right of the app. Extract the ZIP to a writable folder. No installer or separate Visual C++ runtime is needed.

**Upstream's verified setup is Pimax Dream Air** (GeForce RTX 5090 / NVIDIA driver 610.88). A contributor tested Steam Frame on 596.02 before cleanup; see the separate Frame instructions below. See the [full English setup guide](https://github.com/challenger0303/VRSS-Tuner/blob/main/docs/SETUP.md).

## Steam Frame (596.02, experimental)

Use the [Frame-specific instructions](STEAM-FRAME.md), not the legacy EDID setup check or Start-Provider. Enable SteamVR eye-tracking sharing, VR layout 4 and MSAA; close VRChat and Unity, select **Steam Frame > Connect / Retry**, then **Launch VRChat (Frame)**. Vertical conversion is built in; start with Common provider gains X=1, Y=1. Existing Pimax/Baballonia paths remain available.

## First use

1. Install an NVIDIA GPU driver, VRChat, SteamVR, and an eye-tracking source. For Baballonia, send both-eye gaze OSC to `127.0.0.1:9020`.
2. Connect the headset and start SteamVR. Keep VRChat closed. Run `Test-Setup.cmd` to check prerequisites. This diagnostic does not change VRChat or driver settings.
3. If VRSS is Off, save On in the Graphics tab of `VRSS Tuner.exe` and approve the Windows administrator prompt.
4. Run `Start-Provider.cmd` and approve elevation. In Tuner's Gaze Input tab, start the relay and confirm that Baballonia OSC is being received. Then launch VRChat and enable MSAA in its graphics settings.
5. Check in the headset that the high-quality region follows both eyes. Adjust gaze calibration if needed. Run `Stop-Provider.cmd` when finished.

The common provider does not automatically replace an official vendor gaze provider. Use the official path, or stop it through the vendor's normal controls before checking the common path.

## Compatibility limits

The common provider starts only when the NVIDIA driver images match the audited SHA-256 values. Other driver versions are refused. The NVIDIA VRChat profile must have the VRSS allow gate enabled; this package does not fabricate a new profile when it is missing. MSAA, Baballonia OSC output, and headset-specific gaze calibration require manual setup. Validate live tracking on each PC.

`common-vrss-provider/calibration.txt` starts at horizontal/vertical gain 1 and eye offsets 0. Tuner can edit it. Diagnostics and run logs go to `%LOCALAPPDATA%\VRSS Tuner`. NVIDIA and Pimax driver binaries are not included.

Project-authored code is MIT licensed; see `LICENSE.txt`. The bundled OpenVR and NVAPI components retain the terms in `OpenVR-LICENSE.txt` and `NVAPI-LICENSE.txt`. This is not an official NVIDIA, Pimax, Valve, or VRChat product.
