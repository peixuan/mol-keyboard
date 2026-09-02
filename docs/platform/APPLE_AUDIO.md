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
control thread. A media-services reset tears down the old audio object and sets
a status flag; playback is not restarted until user-controlled lifecycle code
starts it again. Playback-category routing is left to the system for wired,
Bluetooth A2DP, and AirPlay outputs.

## Build entry

The existing device and simulator presets add `platforms/apple` automatically:

```text
cmake --preset ios-simulator
cmake --build --preset ios-simulator

cmake --preset ios-device
cmake --build --preset ios-device
```

Link `mol_apple_audio` into the Swift or Objective-C application target and
call `startWithError`, `noteOn`, `noteOff`, and `stop` from serialized lifecycle
code. The future application target must enable the Audio background mode and
use the configurable default bundle identifier `cn.zhangpeixuan.molkeyboard`.

No Apple SDK or Apple host is available on the current Windows machine, so this
entry is source-reviewed only. It is not marked `build-verified` or
`device-verified`; simulator/device compilation, signing, background-mode
packaging, lock-screen playback, and physical route/interruption tests remain
required.
