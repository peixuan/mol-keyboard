# HarmonyOS Native Audio Entry

The HarmonyOS M1 entry follows the required ArkTS-to-Node-API-to-OHAudio path.
`AudioRuntime.ets` owns an external native `AudioHost`; Node-API transfers only
lifecycle calls, bounded note commands, and diagnostic snapshots. ArkTS never
creates, copies, or renders PCM.

The native host requests a 48 kHz stereo float renderer with music usage and
fast latency mode, then queries the effective sample rate, channel count,
format, callback frame size, and latency mode before initializing the same
caller-owned `mol_core` runtime used by the other platforms. The OHAudio write
callback renders directly into the supplied buffer, accepts variable callback
sizes, replaces non-finite values with silence, and performs no allocation,
locking, logging, file access, Node-API call, or stream lifecycle operation.

Output-device changes, forced interruptions, and renderer errors only publish
atomic status and request a restart. ArkTS lifecycle code can observe
`needsRestart` and call `recover()` on its control thread. Underflow, callback,
render, route-change, interruption, error, and non-finite counters are exposed
without crossing into ArkTS from the realtime callback.

## Build entry

Add `platforms/harmony` to the native CMake project used by the HarmonyOS
module. On an OHOS toolchain CMake creates `libmol_harmony_audio.so`, links it
to `libace_napi.z.so` and `libohaudio.so`, and installs it under `lib`.
The application module must package:

- `arkts/AudioRuntime.ets`;
- `arkts/types/libmol_harmony_audio/index.d.ts` as the native module typing;
- the generated `libmol_harmony_audio.so` for each supported ABI.

The implementation uses the independent renderer interrupt and error callbacks
introduced in API 18, so the application must compile and run with a compatible
HarmonyOS API level. The default application bundle name is
`cn.zhangpeixuan.molkeyboard` and remains configurable by the final HAP target.

When `MOL_VALIDATE_MOBILE_SOURCES=ON` on a non-cross-compiling host, CMake
strictly compiles the native host and Node-API module against small
declaration-only SDK subsets under `source_check/include`. Those files mirror
the official signatures used here, provide no implementation, and are never
included by an OHOS build.

## Current verification boundary

The official OpenHarmony multimedia audio headers were inspected at commit
`c2e9f5b`, and the entry passes MSVC Debug and Release source compilation with
warnings as errors. No DevEco Studio/HarmonyOS SDK or physical HarmonyOS device
is available on this machine, so this remains `source-checked`, not
`build-verified` or `device-verified`.

Actual fast-status reporting (rather than the configured latency mode), a
normal-mode retry when fast stream creation fails, AudioSession focus
integration, the official audio-playback continuous-task declaration, HAP
packaging/signing, background playback, and physical route/interruption tests
remain M8 work. The M1 entry does not claim those product-level capabilities.
