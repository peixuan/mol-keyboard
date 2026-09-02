# Apple Native Audio Entry

`MOLAppleAudioHost` is the Objective-C++ entry for iOS and macOS. It configures
an iOS playback `AVAudioSession`, activates a RemoteIO AudioUnit (or the default
output AudioUnit on macOS), queries the effective sample rate and maximum slice
size, and initializes the same caller-owned platform runtime used elsewhere.

The C render callback writes interleaved stereo float PCM directly into the
AudioUnit buffer. It accepts variable frame counts, replaces non-finite values
with silence, and performs no allocation, locking, logging, file access, or
Objective-C message dispatch. Lifecycle and `NSError` work remains on the
control thread.

On iOS the host observes interruption, route-change, and media-services-reset
notifications. Interruption and route recovery are dispatched to the main
control thread. Successful route/interruption recovery notifies the application
controller, which reloads the bounded sequence and persistent controls. A media
services reset tears down invalid objects and lets that controller restart only
when prior user intent and foreground or active background playback still allow
it. Playback-category routing is left to the system for wired, Bluetooth A2DP,
and AirPlay outputs.

## Build entry

The device and simulator scripts first reproduce the Wasm/Web bundle, generate
an Xcode project, build the native application, verify packaged resources, and
lint the final privacy and application property lists:

```text
platforms/ios/build-app.sh Simulator
platforms/ios/build-app.sh Device
```

`mol_ios_app` links `mol_apple_audio` into an Objective-C++ UIKit application.
Its default bundle identifier is `cn.zhangpeixuan.molkeyboard`; override it with
`MOL_APPLE_BUNDLE_IDENTIFIER`. Set `MOL_APPLE_DEVELOPMENT_TEAM` for automatic
Xcode signing, or set `MOL_APPLE_SIGNING_IDENTITY` to sign the completed device
bundle explicitly.

No Apple SDK or Apple host is available on the current Windows machine, so this
application remains source-reviewed rather than `build-verified` or
`device-verified`. See `docs/mobile/M7_IOS_EVIDENCE.md` for the exact completed
implementation and pending Apple acceptance checks.
