# Steam Frame (experimental, NVIDIA 596.02)

The **Steam Frame** tab connects OpenVR eye tracking to VRSS without a separate
provider command window. Existing Pimax, BrokenEye and Baballonia paths remain
unchanged. Connecting is opt-in; merely opening Tuner does not start a worker.

## Use

1. Connect Frame to PC SteamVR. In SteamVR's Steam Link settings, enable sharing
   eye-tracking data with other applications on this PC. OSC is not required.
2. Use the exact [audited 596.02 driver images](DRIVER-CONTRACTS.md), an existing
   VRChat profile permitting VRSS, and VR layout **4**. Other driver versions
   are rejected for the EDID-free Frame session, including the 610.88 provider
   contract retained for other input paths.
3. Close VRChat and Unity. Start the portable **VRSS Tuner.exe** normally, open
   **Steam Frame**, select **Connect / Retry**, and approve the worker's UAC.
4. Once registered, use **Launch VRChat (Frame)**. A normal Steam launch does
   not inherit the required session identifiers. Keep Tuner open while playing.

The Frame path converts the vertical OpenVR coordinate to the convention tested
with VRSS layout 4. Start with Common provider gains **X=1, Y=1**, offsets **0**;
do not add the manual Y inversion used by early experimental builds. No second
projection is applied. Invalid/nonfinite gaze is not replayed.

## Scope and recovery

- The recognized HMD must report manufacturer `Valve` and model `Steam Frame`
  or the observed alias `Deckard MP`. Both EDID properties must be missing.
  Partial/malformed properties and other headsets are rejected.
- `0xffff:0xffff` is a private session key, not a physical EDID. Provider and
  consumer must use the same key. Only the owned worker with a recoverable
  profile transaction can select it.
- The worker temporarily sets the **VRChat-only** HMD gate `0x00524252` to 0.
  It saves the original value before writing, restores and reads it back on
  stop/exit, and checks a pending recovery record on the next connection.
  Conflicting external changes or invalid records stop recovery rather than
  being overwritten. Preserve `frame-gate-pending.txt` if recovery fails.
- An already permitted VRSS profile may be enabled with a settings backup;
  its allow flag, MSAA, layout, global settings and SteamVR settings are not
  changed. Normal settings backup/restore still applies to the enabled mode.
- The standard-rights UI uses the signed installed EAC launcher via
  ShellExecute. `TEST_VENDOR_ID`, `TEST_PRODUCT_ID` and Steam App IDs exist only
  in the UI's launch scope and are restored afterward. No persistent environment
  changes, game/DLL injection, game/EAC patches or anti-cheat guarantees.
- Owner PID/start-time/executable checks, a worker mutex and fresh owned status
  prevent duplicate sessions and stale readiness. UAC cancellation is not
  automatically retried. Closing Tuner unregisters the provider; games are
  never terminated. Logs and recovery state are under
  `%LOCALAPPDATA%/VRSS Tuner/frame-runs/` and its parent directory.

## Validation

The pre-cleanup build was exercised on Steam Frame / SteamVR 2.18.4 / NVIDIA
596.02 in VRChat: gaze sharing, provider/consumer connection, submission,
normal unregister/settings restore, and horizontal/vertical tracking were
observed. The reporter also confirmed with the author that the apparent
softening of very fine grids was expected behavior, not a reversed quality
region. This is not an independent compatibility certification.

The cleaned implementation has offline tests for both driver contracts, HMD
validation, coordinate conversion, stale input/status, owner identity,
cancellation/retry, launch-only environment restoration, and gate recovery
including partial failures and conflicts. The cleaned portable build was also
smoke-tested on Steam Frame / NVIDIA 596.02 / VRChat on 2026-10-26: both UI tabs
displayed correctly, gaze followed in all four directions with gains X=1/Y=1,
81,570 submissions completed with no submission errors, and Stop successfully
unregistered the provider and restored the original gate (read-back verified,
pending recovery record cleared, worker exit code 0).

The retained 610.88 contract was tested offline, not on 610.88 hardware during
this change. Other games, other Frame firmware and future drivers are not
validated. SteamVR overlays such as XSOverlay are not controlled by the VRChat
profile.
