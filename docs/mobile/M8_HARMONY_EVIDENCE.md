# M8 HarmonyOS implementation evidence

## Status

The HarmonyOS application implementation is complete and source-checked. It is
not marked `build-verified`, `runtime-verified`, or `device-verified`: this
Windows host has no DevEco Studio, HarmonyOS SDK/toolchain, signing identity, or
physical HarmonyOS device.

## Implemented product path

~~~text
native ArkUI application and foreground KeyEvent input
  -> AudioService (Preferences, filesDir, AudioSession, AVSession,
                   audio-playback continuous task)
  -> exact, bounded Node-API contract
  -> AudioHost
  -> OHAudio write callback
  -> shared fixed-memory C11 engine
~~~

The application provides:

- a Stage-model API 12 application with `arm64-v8a` and `x86_64` products;
- the complete 30-key touch and foreground physical-key UI plus all music,
  synthesis, effects, transport, recording, and playback controls;
- a strict Node-API boundary with exact arity/type/range checks, bounded event
  batches, bounded sequence transfers, and no ArkTS PCM production;
- OHAudio fast-mode request, explicit normal-mode fallback, actual latency and
  stream-parameter reporting, and an allocation-free native render callback;
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

All JSON and JSON5 project descriptors parse as strict JSON. The HAP build
script passes `bash -n`; its deliberate local run fails closed with an explicit
DevEco requirement rather than generating a pretend artifact. The HarmonyOS
source audit verifies the Stage configuration, supported ABIs, native library,
30 exact key bindings, complete feature controls, private storage, AudioSession,
AVSession, continuous-task calls, and absence of ArkTS `AudioRenderer` or PCM
write code.

MSVC 19.51 compiled both the OHAudio host and Node-API bridge against the
declaration-only SDK source-check boundary with warnings as errors. The full
Debug build passed, and CTest passed 65/65 including
`mol_harmony_project_audit`. The checked official OpenHarmony interface sources
were used to confirm every native and ArkTS API signature referenced by the
application; this is still source review, not a HarmonyOS build.

## Reproducible HarmonyOS commands

On a machine with DevEco Studio and a HarmonyOS API 12 or newer SDK/native
toolchain:

~~~bash
platforms/harmony/build-app.sh debug
platforms/harmony/build-app.sh release
~~~

The script discovers DevEco through `HVIGORW` or `DEVECO_HOME`, invokes Hvigor,
finds the generated HAP, and audits its contents. Supply the product-specific
bundle identity and signing configuration through the DevEco product settings
for installation on a real device.

## Pending real HarmonyOS acceptance

The following remain mandatory before status promotion:

1. Real DevEco warnings-as-errors Debug and Release HAP builds for both declared
   ABIs, including package-content and signing/archive inspection.
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
