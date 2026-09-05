# Third-Party Notices

## miniaudio 0.11.25

MoL Keyboard uses miniaudio's device-I/O layer for desktop playback. The exact
upstream commit is `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`. MoL Keyboard
elects the MIT No Attribution (MIT-0) option offered by the upstream project.

Copyright 2025 David Reid

The complete upstream dual-license notice is preserved at
`third_party/licenses/miniaudio-LICENSE.txt`. No local patches are applied.

## Oboe 1.10.0

MoL Keyboard uses Oboe for Android low-latency output. The exact upstream
commit is `a81bb9f87d4105b84b682685d3bfbb5beca371d1` and no local patches are
applied. Oboe is licensed under Apache License 2.0; the complete upstream
license is preserved at `third_party/licenses/oboe-LICENSE.txt`.

## Playwright 1.62.1

MoL Keyboard uses Playwright and `@playwright/test` only as development-time
browser automation. The npm packages and browser revisions are exact-lock
dependencies; downloaded browser binaries remain outside Git and are not part
of application distributions. Playwright is licensed under Apache License 2.0.
The complete upstream license and Microsoft notice are preserved at
`third_party/licenses/playwright-LICENSE.txt`. No local patches are applied.

## Android build and Kotlin runtime

The Android project uses the Gradle 8.11.1 wrapper and Android Gradle Plugin
8.10.1 as build-time tools. The wrapper distribution and checked-in wrapper JAR
are checksum locked. The packaged application uses Kotlin Standard Library
2.1.20 and its transitive JetBrains Java Annotations 13.0 dependency. These
components are licensed under Apache License 2.0; the complete license text is
the repository's `LICENSE`. Artifact checksums and source locations are
recorded in `third_party/manifest.lock.json` and the SPDX SBOM. No local
patches are applied.

## wxWidgets 3.2.11

The optional Windows, Linux, and macOS desktop GUI targets use wxWidgets from
the official 3.2.11 source archive. It is statically linked as separate base,
core, and WebView libraries and is never linked into `mol_core`,
`mol-keyboardd`, or `molctl`. Bundled image, media, rich-text, XRC, OpenGL,
sample, demo, benchmark, and test components are disabled. No local patches are
applied. wxWidgets is licensed under the wxWindows Library Licence 3.1, including
its binary distribution exception; the complete upstream text is preserved at
`third_party/licenses/wxwidgets-LICENCE.txt`.

## Microsoft Edge WebView2 SDK 1.0.3485.44

The Windows wxWebView backend uses the checksum-locked Microsoft Edge WebView2
SDK selected by wxWidgets 3.2.11. The package supplies WebView2 headers and the
dynamically loaded `WebView2Loader.dll`; the Evergreen browser runtime is a
system prerequisite and is not bundled. The SDK license and third-party notice
are preserved at `third_party/licenses/webview2-LICENSE.txt` and
`third_party/licenses/webview2-NOTICE.txt`. No local patches are applied.
