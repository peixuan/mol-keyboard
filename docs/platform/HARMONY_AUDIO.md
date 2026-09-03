# HarmonyOS audio application

The HarmonyOS application follows the required ArkTS-to-Node-API-to-OHAudio
path. `AudioService.ets` owns an external native `AudioHost`; Node-API transfers
only lifecycle calls, bounded control commands, sequence bytes, event batches,
and diagnostic snapshots. ArkTS never creates, copies, or renders PCM.

~~~text
ArkUI controls and foreground KeyEvent input
  -> AudioService (focus, AVSession, continuous task, private storage)
  -> strict Node-API boundary
  -> OHAudio API 12 S16 write callback
  -> shared fixed-memory ISO C11 engine
~~~

The native host first requests a 48 kHz stereo S16 renderer with music usage
and `AUDIOSTREAM_LATENCY_MODE_FAST`. If fast stream creation fails it retries
with normal latency rather than hiding the failure. It then queries and reports
the effective sample rate, channel count, format, callback frame size, renderer
state, and actual latency mode before initializing the same caller-owned
`mol_core` runtime used by the other platforms.

The OHAudio write callback renders bounded chunks into a fixed stack buffer,
converts them to S16 in the supplied output buffer, accepts variable callback
sizes, replaces non-finite values with silence, and performs no heap allocation,
locking, logging, file access, Node-API call, or stream lifecycle operation.
Output-device changes, forced interruptions, and renderer errors publish atomic
status and request a control-thread rebuild. Persistent controls, the loaded
sequence, transport, and eligible playback are restored after that rebuild.

## Application project

The Stage-model project is under `platforms/harmony/app`. It targets HarmonyOS
API 12 and packages `libmol_harmony_audio.so` for `arm64-v8a` and `x86_64`. The
default bundle identifier is `cn.zhangpeixuan.molkeyboard`; a product build may
override it.

The ArkUI surface exposes the exact 30 note bindings, Space sustain, all 18
presets, octave and transpose, eight scales, ten chord modes, seven arpeggiator
modes, tempo/meter/metronome, portamento, recording, sequence playback, and
diagnostics. Foreground hardware keys and touch use the same command model.
Settings use Preferences. Recordings use private `filesDir` storage with a
2 MiB bound and temp-file, `fsync`, rename replacement.

Audio focus is acquired with `AudioSessionManager`. An AVSession publishes
play/pause state and accepts media commands. The official audio-playback
continuous task is started only after a user action and only while sequence
playback or metronome plus running transport needs sound. Idle backgrounding
stops the stream, deactivates the audio session, and releases the AVSession.

## Reproducible build

The official public OpenHarmony API 12 compatibility lane can be built on
Windows or Linux after setting `HVIGORW` and `OHOS_BASE_SDK_HOME` and placing a
JDK on `PATH`:

~~~powershell
platforms/harmony/build-openharmony-compat.ps1 Debug
platforms/harmony/build-openharmony-compat.ps1 Release
~~~

Use `build-openharmony-compat.sh debug|release` on POSIX hosts. These wrappers
sync the formal application's shared sources into an ignored compatibility
module, build real ArkTS and native outputs, and require both ABI libraries in
the resulting unsigned HAP.

Install DevEco Studio with a HarmonyOS API 12 or newer SDK and native toolchain,
then run from the repository root:

~~~bash
platforms/harmony/build-app.sh debug
platforms/harmony/build-app.sh release
~~~

Set `HVIGORW` to an actual DevEco `hvigorw` executable, or set `DEVECO_HOME`, if
it is not discoverable. The script invokes the real Stage build and rejects a
result unless a non-empty HAP contains the module profile, ArkTS bytecode or
module content, and `libmol_harmony_audio.so` for a packaged ABI. It never
substitutes a desktop CMake build for a HarmonyOS package.

When `MOL_VALIDATE_MOBILE_SOURCES=ON` on a non-cross-compiling host, CMake also
strictly compiles the native host and Node-API module against small
declaration-only SDK subsets under `source_check/include`. Those declarations
mirror only the official signatures used here, provide no implementation, and
are never included by an OHOS build.

## Verification boundary

The full application project, native bridge, OHAudio host, ArkUI surface,
official lifecycle integrations, persistence, and audited HAP pipeline are
implemented. The public OpenHarmony 5.0.0.71/API 12 toolchain builds and audits
Debug and Release compatibility HAPs with ArkTS bytecode and both AArch64 and
x86-64 native libraries. Windows MSVC and Linux Clang also compile the C++
boundary with warnings as errors, and the repository audit checks the Stage
declarations, exact keyboard table, complete control surface, private
persistence, continuous-task/AVSession/AudioSession calls, and absence of ArkTS
PCM rendering.

No DevEco Studio/HarmonyOS SDK, signing identity, emulator, or physical device
is available on the current host. Therefore only the OpenHarmony compatibility
artifact is `build-verified`; the formal HarmonyOS product remains
`implementation-complete` and `source-checked`, and neither is
`runtime-verified` or `device-verified`. Formal HAP construction, installation,
audible performance, background playback, interruptions, output-route changes,
latency, and sustained playback remain mandatory M8 acceptance work. See
`docs/mobile/M8_HARMONY_EVIDENCE.md`.

## Platform references

- OHAudio native APIs:
  https://developer.huawei.com/consumer/en/doc/harmonyos-references-V13/native__audio__session__manager_8h-V13
- Audio playback continuous tasks:
  https://developer.huawei.com/consumer/en/doc/harmonyos-references-V5/js-apis-resourceschedule-backgroundtaskmanager-V5
- AudioRenderer background playback guidance:
  https://developer.huawei.com/consumer/cn/doc/doccenter-feature-dev/bpta-playing-pcm-audio-based-audiorenderer
- ArkUI hardware-key events:
  https://developer.huawei.com/consumer/en/doc/harmonyos-guides-V13/arkts-common-events-device-input-event-V13
- Native HarmonyOS project integration:
  https://developer.huawei.com/consumer/en/doc/harmonyos-guides-V5/build-with-ndk-ide-V5
