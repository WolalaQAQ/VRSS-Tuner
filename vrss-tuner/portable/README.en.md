# VRSS Tuner Portable

This is an experimental Windows app for NVIDIA VRSS2 settings and gaze input. Extract the ZIP to a writable folder. Switch Japanese/English at the top right of the tuner.

**The maintainer's verified setup is Pimax Dream Air** (GeForce RTX 5090 / NVIDIA driver 610.88). A contributor also smoke-tested the cleaned Steam Frame route on 596.02; see the separate Frame instructions below. See the [full English setup guide](https://github.com/challenger0303/VRSS-Tuner/blob/main/docs/SETUP.md).

## First steps

1. Extract the **whole ZIP**, open `VRSS Tuner.exe`, and choose your language at the top right. Install the NVIDIA driver, headset eye-tracking software, SteamVR, and VRChat separately.
2. For **native Pimax XR5**, enable Pimax eye tracking, select **Pimax path (native priority)** and **VRSS On** in the tuner, then apply. Do not run `Start-Provider.cmd` or the common-path `Test-Setup.cmd`.
3. For **Steam Frame**, follow the Frame-specific instructions below. Do not run a common-path launcher.
4. For **other gaze sources**, connect the headset, start its gaze software, enable VRSS and select a source in the tuner, then click **Start common path + VRChat**. The launcher first tries to connect to an already-running VRChat, and restarts it normally only if NVIDIA does not report a consumer. It also starts SteamVR if needed. Windows administrator confirmation still needs a manual click.
5. Enable MSAA in VRChat's graphics settings. Verify in the headset that the high-quality region follows gaze in both eyes. A green tuner status alone does not prove the rendered result.

Other PCs and untested headsets are not guaranteed to work. The common path requires an audited driver image and an NVIDIA VRChat profile that allows VRSS. If setup fails, use its reported reason.

## Steam Frame (596.02, experimental)

Use the [Frame-specific instructions](STEAM-FRAME.md), not the legacy EDID setup check or Start-Provider. Enable SteamVR eye-tracking sharing, VR layout 4 and MSAA; close VRChat and Unity, select **Steam Frame > Connect / Retry**, then **Launch VRChat (Frame)**. Vertical conversion is built in; start with Common provider gains X=1, Y=1. This is separate from the generic SteamVR eye-action route below.
## Choose one gaze source

For the common path, connect the headset and start its gaze software. Turn on VRSS in the tuner, then choose **one** source. The tuner button starts SteamVR if needed, checks setup, and starts the provider. If VRChat is already running, it waits about eight seconds for an NVIDIA consumer before restarting VRChat. Candidate HMD identities require a restart. If VRChat does not accept a normal close, the launcher stops without forcing termination. The native Pimax path does not use this setup check or launcher.

| Source | Tuner selection | How to start |
| --- | --- | --- |
| Native Pimax XR5 | Pimax path (native priority) | Use Pimax's normal eye tracking; no common provider |
| Pimax + SRanibro CUSTOM | Pimax path (native priority) | Requires the Pimax eye bridge to be installed; mutually exclusive with native XR5 |
| Baballonia binocular OSC | Baballonia OSC | Run `Start-Provider.cmd`, then Start relay in the tuner; OSC destination is `127.0.0.1:9020` |
| PSVR2 Toolkit CAPI | PSVR2 Toolkit (direct) | Start a compatible installed Toolkit, then run `Start-PSVR2-Toolkit.cmd`; Toolkit itself is not bundled |
| SRanibro binocular gaze | SRanibro (common) | Start SRanibro, then run `Start-SRanibro.cmd`; this route is shared across headsets |
| VIVE Pro Eye | VIVE Pro Eye (SRanipal; hardware unverified) | Calibrate SRanipal eye tracking, then run `Start-Vive-Pro-Eye.cmd`; SRanipal is not bundled |
| VIVE PC Streaming headsets | VIVE Streaming (hardware unverified) | Enable gaze forwarding in VIVE Streaming, then run `Start-Vive-Streaming.cmd`; its SDK is not bundled |
| Quest Pro via Steam Link; other experimental SteamVR sources | SteamVR eye action (hardware unverified) | Enable PC eye-data sharing, then run `Start-SteamVR-Eye.cmd`; the single fixation ray is projected into both eyes. For Steam Frame use the dedicated tab above. |
| Varjo XR-4 / Aero / XR-3 / VR-3 | Varjo Native SDK (hardware unverified) | Run Varjo Base and `Start-Varjo.cmd`; the installed `VarjoLib.dll` is required |

The tuner's **Start common path + VRChat** button opens the matching script. `Start-Provider.cmd` remains available for provider-only manual diagnostics. If the VIVE DLL is outside the standard installation directories, set `VRSS_VIVE_SDK_DLL` to the full path of `SRanipal.dll` or `VS_PC_SDK.dll`. For Varjo, set `VRSS_VARJO_DLL` to the installed `VarjoLib.dll` if needed; no vendor DLL is bundled. A missing DLL stops before the administrator prompt. If Pro Eye reports only one valid eye, the bridge projects that fresh direction into both eye views. VIVE, SteamVR eye action and Varjo routes have passed code/synthetic tests only; VRChat gaze tracking remains unverified. Varjo gains are read at launch and its per-eye angle offsets are not yet applied.

The four stages in **Gaze input** monitor the route actually running, across source selections. Green means that stage was observed, amber means a running path still needs verification, and gray means stopped or waiting for data. For Baballonia, input reception and forwarding are separate. For SRanibro, PSVR2 Toolkit, VIVE, SteamVR eye action and Varjo, the source process, fresh gaze reaching the common provider, provider registration, and NVIDIA acceptance are separate checks. When the selected and running routes differ, both are shown. Even a green final stage does not prove a visible VRSS image change.

The blue rings show actual gaze coordinates; green dots magnify motion for display only. Display zoom is 1–4x and does not change gaze data, calibration, or the VRSS region. On the common path, fresh provider input moves the dots even before VRChat requests gaze; the caption distinguishes **provider input** from **NVIDIA-accepted gaze**. On the Pimax path, the preview uses NVIDIA queue coordinates when readable and falls back to Pimax SDK input otherwise. The dots are not the high-quality region boundary, so verify rendering inside the headset. Selecting a source does not switch a running provider; restart VRChat after applying VRSS settings.

Starting the common provider requires Windows administrator confirmation. Launch VRChat, enable MSAA, and check gaze reception and NVIDIA acceptance in the tuner. The dots represent submitted gaze coordinates, not the rendered high-quality region. Run `Stop-Provider.cmd` when finished. That stop request also ends the Toolkit, VIVE, SteamVR and Varjo bridges.

`Test-Setup.cmd` first reads the HMD's OpenVR EDID identity. If EDID is absent but OpenVR provides a stable tracking-system name and serial number, it derives an experimental ID and reports `hmdIdentitySource=candidate-auto`. It stores a comparison hash, not the serial number. If neither identity is available, setup stops. After starting the common provider with the candidate, launch protected VRChat with `Start-VRChat-Candidate.cmd`, not the ordinary Steam launch. The launcher rechecks the connected HMD and passes the provider's exact ID to VRChat. This requires the audited NVIDIA HMD test branch in the VRChat profile.

For a manual experiment, `VRSS_HMD_ID_CANDIDATE` still accepts a **decimal** `vendor:product` pair. Setup reports `candidate-manual` and the same environment value must be present at VRChat launch. Neither candidate mode proves VRSS2 works on another headset. Check `consumerCount`, fresh accepted gaze, then a rendered Force/Off comparison in the headset. The derived 16-bit ID is experimental and could collide with another device.

See the [HMD compatibility table](HMD-GAZE-COMPATIBILITY.md) for implementation and live-test status by headset.

The common SRanibro route reads the local BrokenEye-format stream on `127.0.0.1:5555` and uses the connected headset's OpenVR projection geometry and tuner's calibration. Headsets may use different gaze conventions, so calibrate and verify each one. It will not work unless SRanibro outputs valid binocular gaze in that format. If an official Pimax provider is active, select that route or switch through the vendor's normal controls before using the common provider.

Face-tracking coexistence is audited in [FACE-TRACKING-COEXISTENCE.md](FACE-TRACKING-COEXISTENCE.md) (Japanese). The PICO single-client diagnostic probe cannot share its session with the VRCFT PICO module. Concurrent VIVE and PSVR2 hardware operation has not been verified yet.

This full release-candidate ZIP includes the Pimax eye bridge at `pimax-eye-bridge/driver_sranibro_eye.dll`; it is not installed automatically. When only one eye has valid gaze, the bridge uses its fresh direction for both eye inputs. It submits no gaze when both eyes are invalid or data is stale. In-headset alignment of this monocular fallback is still being validated. SRanibro and the NVIDIA/Pimax drivers are not bundled. The Varjo bridge is also included, while the Varjo runtime must be installed separately. Check `bundle-manifest.json` for exact files and hashes. This build does not establish live headset compatibility for untested routes.

## Limits and files

The common provider accepts two built-in audited NVIDIA driver-image pairs. `Audit-Driver.cmd` records a different driver's hashes and method addresses without private registration; after independent ABI review, an advanced tester can opt into an exact experimental contract as described in [DRIVER-CONTRACTS.md](DRIVER-CONTRACTS.md). Other images are refused. The VRChat profile also needs its VRSS allow gate. MSAA, valid binocular gaze, and per-headset calibration are necessary for a live result. Initial calibration is copied from `common-vrss-provider/calibration.txt`; edits persist in `%LOCALAPPDATA%\VRSS Tuner\common-calibration.txt` across portable updates. Diagnostics and logs use the same folder.

Project code is MIT licensed. The bundled OpenVR, NVAPI, and PSVR2 Toolkit header retain their respective license terms in the accompanying license files. This is not an official NVIDIA, Pimax, Sony, Valve, or VRChat product.
