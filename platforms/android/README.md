# MoL Keyboard for Android

This application packages the complete local Web UI while all sound is rendered
by the shared ISO C11 engine through JNI and Oboe. PCM never enters Kotlin or
the WebView. Android 8.0 (API 26) is the minimum; API 36 is the compile and
target SDK.

## Reproducible build

Install JDK 17 or 21 and these exact Android SDK packages:

- Platform 36 revision 2
- Build Tools 36.0.0
- NDK 28.2.13676358
- CMake 3.31.6

Activate the pinned Emscripten 6.0.5 environment, set `ANDROID_HOME` (or
`ANDROID_SDK_ROOT`), and run:

```powershell
platforms/android/build-app.ps1 Debug
platforms/android/build-app.ps1 Release
```

On POSIX hosts, use `platforms/android/build-app.sh`. Both scripts build and
test the Wasm core, install the exact npm lock, test and bundle the shared UI,
then invoke the checked-in Gradle 8.11.1 wrapper. The wrapper distribution has
the required SHA-256 in `gradle/wrapper/gradle-wrapper.properties`. Gradle pins
Android Gradle Plugin 8.10.1 and Kotlin 2.1.20; the app pins both required ABIs.

`MOL_SKIP_WEB_BUILD=1` is only for a previously verified `apps/web/dist`.
`MOL_ANDROID_AAPT2_OVERRIDE` may name the SDK's verified `aapt2` executable
on a network-restricted host; normal builds use the standard Maven artifact.

Outputs:

```text
app/build/outputs/apk/debug/app-debug.apk
app/build/outputs/apk/release/app-release-unsigned.apk
app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
```

The release APK is release-ready but intentionally unsigned. Production
signing credentials must stay outside the repository.

The default application ID can be overridden without changing the Kotlin/JNI
namespace:

```powershell
platforms/android/gradlew.bat -p platforms/android :app:assembleRelease `
  -PmolApplicationId=org.example.molkeyboard
```

## Device validation

The no-dependency instrumentation APK exercises the packaged UI, product start
button, strict JavaScript bridge, JNI, Oboe stream, shared core, note commands,
callback counters, foreground-service notification state, background playback,
screen-off continuation, and idle-background shutdown:

```powershell
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w \
  cn.zhangpeixuan.molkeyboard.test/cn.zhangpeixuan.molkeyboard.AndroidSmokeInstrumentation
```

Long-duration playback, wired/Bluetooth route changes, audio focus
interruptions, and physical-keyboard input remain physical-device checks.
Ordinary hardware keyboard input is supported only while the activity is
foregrounded. Bluetooth audio pairing and routing belong to Android; the
application responds to the resulting system route changes.

## Privacy and permissions

The application has no `INTERNET` permission and loads only packaged assets
from a constrained HTTPS app origin. It requests only foreground media
playback and notification permissions. It has no account, analytics,
advertising, telemetry, backup, or recording upload. `PRIVACY.md` and
`THIRD_PARTY_NOTICES.md` are packaged in every APK.
