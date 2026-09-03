# M7 iOS implementation evidence

## Status

The iOS application implementation is complete and source-reviewed. Its exact
production background-policy state machine is executable under non-Apple
toolchains, but the application is not marked `build-verified`,
`runtime-verified`, or `device-verified`: this Windows host has no Xcode, iOS
SDK, Simulator, Apple signing identity, or physical Apple device.

## Implemented product path

~~~text
packaged production Web UI
  -> WKScriptMessageHandlerWithReply (version 1, exact schema)
  -> Objective-C++ application controller
  -> MOLAppleAudioHost
  -> RemoteIO render callback
  -> shared fixed-memory C11 engine
~~~

The application provides:

- a UIKit application and generated Xcode project for iPhone and iPad;
- the complete local UI under `mol-keyboard://app/`, with normalized bundle
  paths, main-frame origin checks, blocked HTTP(S)/WebSocket subresources, and
  no remote dependency;
- an asynchronous WKWebView bridge with allow-listed methods and commands,
  finite/ranged numeric validation, 2 MiB recording limits, bounded event
  batches, and bounded error text;
- 30 foreground hardware-key mappings plus Space sustain through UIKit HID
  usages, with key-repeat suppression and release on app deactivation;
- playback-category AVAudioSession configuration, RemoteIO, effective sample
  rate/slice reporting, interruption handling, route rebuild, and media
  services reset recovery;
- restoration of loaded sequences, persistent controls, transport, and
  playback after a system audio rebuild;
- background continuation only after a user start and only while sequence
  playback or a running metronome transport actually needs audio; idle
  backgrounding releases AudioUnit and AVAudioSession;
- atomic `.molseq` persistence in Application Support plus private WebKit
  IndexedDB storage;
- localized display names, a real app icon, the `audio` background mode, an
  empty-data/no-tracking privacy manifest, and configurable bundle/signing
  settings.

The AudioUnit callback contains no Objective-C dispatch, allocation, locks,
logging, file access, or lifecycle work. It accepts variable slice sizes,
silences invalid buffers, sanitizes non-finite output, and reports counters
through atomics.

## Local validation on 2026-09-03

The Objective-C++ controller directly consumes
`mol_ios_audio_lifecycle.c`; this is not a separate test model. Its executable
tests cover user-start gating, foreground resignation, idle background stop,
playback continuation and completion, the metronome-plus-transport rule,
engine reset, route restoration, foreground/background media-services reset,
restart failure, route-revision saturation, and invalid/null calls. The same
production C source passed as part of Windows MSVC Release 80/80, Linux GCC
80/80, Emscripten MinSizeRel 33/33, and ASan/UBSan 47/47 suites; Clang static
analysis also includes this production translation unit. This validates
deterministic application policy only; it does not simulate or claim UIKit,
AVAudioSession, RemoteIO, OS notifications, actual background scheduling, or an
audio route.

The source was formatted with Visual Studio ClangFormat 22 and passed
`git diff --check`. The property lists parsed as XML, the asset catalogs parsed
as JSON, and `build-app.sh` passed `bash -n` under WSL. The 1024×1024 app icon
was rasterized from the repository's clean-room SVG and inspected at native
resolution.

The shared Web UI passed 12/12 Node tests, strict TypeScript checking, and a
production Vite build after adding Promise-based WKWebView reply support.
MSVC Release rebuilt the complete native project with warnings as errors and
passed 80/80 CTest cases, including the production iOS lifecycle state machine,
macOS platform simulations, and dependency license audit.

## Reproducible Apple commands

On a Mac with Xcode and the pinned Emscripten 6.0.5 environment active:

~~~bash
platforms/ios/build-app.sh Simulator
MOL_SKIP_WEB_BUILD=1 platforms/ios/build-app.sh Device
~~~

The scripts require a real generated `MoLKeyboard.app`, executable, compiled
`Assets.car`, privacy manifest, application property list, and packaged Web
entry. They lint both property lists. CI executes both unsigned configurations
on `macos-15`. Supply `MOL_APPLE_DEVELOPMENT_TEAM`,
`MOL_APPLE_BUNDLE_IDENTIFIER`, and optionally
`MOL_APPLE_SIGNING_IDENTITY` for a signed device bundle.

## Pending real Apple acceptance

The following remain mandatory before status promotion:

1. Xcode warnings-as-errors simulator and arm64 device builds.
2. Simulator launch of the packaged UI and strict bridge rejection tests.
3. Physical audible note/record/playback with finite callback counters.
4. Background and lock-screen callback advance only for active playback or
   metronome transport, followed by idle shutdown.
5. Interruption, media-services reset, wired/Bluetooth/AirPlay route changes,
   and state restoration.
6. Foreground physical hardware keyboard note/sustain/repeat/release behavior.
7. Wired latency, Bluetooth informational latency, sustained playback,
   signing, installation, and privacy archive inspection.

Ordinary hardware keyboard input is intentionally not promised while iOS has
the application fully backgrounded. System output pairing and routing remain
owned by iOS.

## Platform references

- Apple `AVAudioSessionCategoryPlayback` documentation:
  https://developer.apple.com/documentation/avfaudio/avaudiosession/category-swift.struct/playback
- Apple background audio guidance:
  https://developer.apple.com/documentation/avfaudio/configuring-your-app-for-media-playback
- Apple reply-capable script bridge:
  https://developer.apple.com/documentation/webkit/wkscriptmessagehandlerwithreply
- Apple UIKit keyboard HID usages:
  https://developer.apple.com/documentation/uikit/uikeyboardhidusage
