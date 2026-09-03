# M8 HarmonyOS implementation evidence

## Status

The HarmonyOS application implementation is complete and source-checked. Its
exact production OHAudio host now passes executable controlled-API simulations,
including an AArch64 QEMU run. A separate OpenHarmony API 12 compatibility
product is `build-verified` with the official public OpenHarmony SDK, but this
does not qualify the formal HarmonyOS/DevEco product. Neither product is
`runtime-verified` or `device-verified`: this Windows host has no DevEco Studio,
HarmonyOS SDK, signing identity, emulator, or physical HarmonyOS/OpenHarmony
device.

## Implemented product path

~~~text
native ArkUI application and foreground KeyEvent input
  -> AudioService (Preferences, filesDir, AudioSession, AVSession,
                   audio-playback continuous task)
  -> exact, bounded Node-API contract
  -> AudioHost
  -> OHAudio API 12 S16 write callback
  -> shared fixed-memory C11 engine
~~~

The application provides:

- a Stage-model API 12 application with `arm64-v8a` and `x86_64` products;
- the complete 30-key touch and foreground physical-key UI plus all music,
  synthesis, effects, transport, recording, and playback controls;
- a strict Node-API boundary with exact arity/type/range checks, bounded event
  batches, bounded sequence transfers, and no ArkTS PCM production;
- OHAudio fast-mode request, explicit normal-mode fallback, actual latency and
  stream-parameter reporting, and an allocation-free native render callback
  that converts bounded float render chunks to API 12 S16 PCM;
- audio focus through AudioSession, media state/commands through AVSession, and
  the official audio-playback continuous-task lifecycle;
- background continuation only after user initiation and only for active
  sequence playback or metronome plus running transport, with idle release;
- route, forced-interruption, renderer-error, and AudioSession deactivation
  recovery with persistent controls, sequence, transport, and playback restore;
- Preferences-backed settings and atomic private `.molseq` persistence with a
  2 MiB limit; and
- bilingual resources, offline operation, a clean-room app icon, configurable
  bundle identity, and a HAP build/audit script that rejects incomplete output.

## Local validation on 2026-09-03

The formal HarmonyOS product descriptor and build lane remain intact. An
isolated `openharmony` product and `entryOpenHarmony` module reuse generated
copies of its ArkTS, resource, and native entry sources while narrowing the
public-SDK descriptor to the tablet device type. The checked-in PowerShell and
POSIX wrappers recreate
those ignored copies before each build and reject a HAP unless it contains the
module profile, resources, ArkTS bytecode, and both native ABIs.

The official OpenHarmony 5.0.0 Release public SDK 5.0.0.71 (API 12), Hvigor
5.8.9 with the 5.8.9 OHOS plugin, Node.js 22.16.0, and Microsoft OpenJDK
21.0.12.1 built both Debug and Release compatibility HAPs. The SDK archive
SHA-256 was
`F06A2A8AE38A3CF01C583557F5A4BB1E6E3626DF975599AA71F4A59E9AF70ECC`,
matching its published checksum. The Release output was an unsigned 2,951,842
byte HAP with SHA-256
`56DC8642463BA3550940D022C1810CB3D540045E606AF164A0E0ED6438A93439`.
Its 13 audited entries include `ets/modules.abc`, `module.json`,
`resources.index`, and `libmol_harmony_audio.so` for `arm64-v8a` and `x86_64`.
LLVM inspection identifies those libraries as AArch64 and x86-64 ELF64 shared
objects and confirms their Node-API registration plus API 12 OHAudio imports.

The unchanged production `AudioHost` also executes against a controlled API 12
OHAudio implementation. The tests verify the requested 48 kHz stereo S16 music
contract, fast renderer startup, normal-latency fallback, effective stream
reporting, 640-frame chunked non-silent PCM, malformed-buffer silencing,
underflow reporting, route loss, shared and forced interruption behavior,
renderer errors, clean recovery/release, invalid commands, and real engine
record/export/load/playback. Failure injection rejects bad negotiated formats,
renderer creation, and renderer startup. MSVC Release, Linux x86_64 GCC,
Emscripten, and Clang ASan/UBSan execute this exact host. The same test also
cross-compiles with GNU 15.2.0 and passes under AArch64 QEMU as part of the
current 69/69 target suite. These are host API and target-instruction
simulations, not HarmonyOS runtime or device evidence.

ArkTS strict checking found and drove fixes for explicitly typed selector data,
the component `scale` name collision, API 12 AudioSession/WantAgent signatures,
and asynchronous Preferences flush. Native cross-compilation found and drove
the API 12 S16 callback and aggregate renderer-callback implementation. The
remaining compatibility compiler warnings identify native-module declaration
verification and tablet-dependent background-task capabilities; the Node-API
declaration is packaged, and background-task failure remains explicitly
handled, but only device execution can settle those runtime paths.

MSVC 19.51 and Linux Clang 21.1.8 also compile the OHAudio host and Node-API
bridge against the declaration-only API 12 source-check boundary with warnings
as errors. The current Windows, Linux, and Emscripten suites pass 87/87, 88/88,
and 39/39 respectively, including the executable host simulation and
`mol_harmony_project_audit`; both build wrappers pass shell syntax checks. This
is real OpenHarmony package evidence plus controlled host simulation, not a
formal HarmonyOS build or runtime/device result.

## Reproducible HarmonyOS commands

For the public OpenHarmony compatibility lane, set `HVIGORW`,
`OHOS_BASE_SDK_HOME`, and a JDK on `PATH`, then run:

~~~powershell
platforms/harmony/build-openharmony-compat.ps1 Debug
platforms/harmony/build-openharmony-compat.ps1 Release
~~~

The equivalent POSIX commands are
`platforms/harmony/build-openharmony-compat.sh debug` and `release`.

On a machine with DevEco Studio and a HarmonyOS API 12 or newer SDK/native
toolchain, build the formal product with:

~~~bash
platforms/harmony/build-app.sh debug
platforms/harmony/build-app.sh release
~~~

The formal script discovers DevEco through `HVIGORW` or `DEVECO_HOME`, invokes
Hvigor, finds the generated HAP, and audits its contents. Supply the
product-specific bundle identity and signing configuration through the DevEco
product settings for installation on a real device.

## Pending real HarmonyOS acceptance

The following remain mandatory before status promotion:

1. Real DevEco Debug and Release HAP builds of the formal HarmonyOS product for
   both declared ABIs, including package-content and signing/archive inspection.
2. Installation and foreground touch/physical-key performance on a physical
   HarmonyOS device with audible notes, all 18 presets, recording, and playback.
3. Verification that the effective fast/normal latency report matches the
   stream and wired measurements; Bluetooth latency remains informational.
4. Background and screen-off callback advance only during eligible active
   audio, media-session control behavior, and prompt idle shutdown.
5. Audio focus loss/gain, forced interruption, renderer error, wired/Bluetooth
   output-route change, and state restoration.
6. Sustained finite playback with no realtime violations or unbounded memory
   growth.

Ordinary physical-key input is intentionally promised only while the ArkUI
application can legally receive foreground key events. Pairing and routing of
Bluetooth speakers remain owned by HarmonyOS.
