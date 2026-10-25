# VRSS Tuner

[日本語ガイド](README.ja.md) · [English setup guide](docs/SETUP.md) · [日本語セットアップガイド](docs/SETUP.ja.md)

VRSS Tuner is an experimental Windows tool for configuring NVIDIA VRSS2 and sending binocular gaze data to a common VRSS provider. It can receive Baballonia OSC gaze data and uses OpenVR headset geometry for per-eye mapping. It does not modify VRChat files or inject a DLL into the game.

**Verified headset:** Pimax Dream Air, with the setup used by the developer. Other headsets and PCs have not been verified. This is an independent project, not an NVIDIA, Pimax, Valve, or VRChat product.

PSVR2 is a [candidate input route](docs/SETUP.md#psvr2-input-route-unverified) through the common provider. The tuner now displays each live gaze-path stage, but VRSS tracking on PSVR2 remains unverified.

The portable package includes the tuner, common gaze provider, OpenVR geometry helper, bilingual instructions, and a read-only setup check. It requires an NVIDIA driver matching the audited driver binaries (610.88 in the verified setup), a VRChat NVIDIA profile with the VRSS allow setting, SteamVR, a gaze source, and MSAA enabled in VRChat. Unsupported driver builds are refused because the provider uses an undocumented driver interface. Passing the setup check does not establish that the foveated region tracks correctly in a headset.

## Get started

1. Download the ZIP from [Releases](https://github.com/challenger0303/VRSS-Tuner/releases) and extract it to a writable folder.
2. Connect the HMD, start SteamVR, leave VRChat closed, and run `Test-Setup.cmd`.
3. Follow the [setup guide](docs/SETUP.md) to enable VRSS, start the provider and gaze relay, then launch VRChat with MSAA enabled.

If the check reports an unsupported driver or missing VRSS allow setting, do not force the provider to start. Open an issue with the diagnostic output, driver version, and HMD model; remove personally identifying information first.

## Build from source

Install Visual Studio 2022 with **Desktop development with C++**. From PowerShell run `vrss-tuner\test.ps1` for isolated tests or `vrss-tuner\Build-Portable.ps1` for a portable ZIP. The build scripts locate `vcvars64.bat` with `vswhere`; `VCVARS64_BAT` can override it. The repository contains the necessary NVAPI and OpenVR build files under `third_party` with their own notices.

The project code is MIT licensed. See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md). NVIDIA and Valve components retain their respective licenses.
