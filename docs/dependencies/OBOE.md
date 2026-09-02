# Oboe Dependency Audit

The Android audio host uses Oboe 1.10.0 from the official `google/oboe`
repository at commit `a81bb9f87d4105b84b682685d3bfbb5beca371d1`.
CMake downloads the signed release tag archive and requires SHA-256
`0e4245f8860c4287040a5d76501c588490bcc9cb57614c486c0c201a5dde3e9f`.

## License review

The release is licensed under Apache License 2.0. Its top-level `LICENSE` was
inspected from the pinned archive and preserved byte-for-byte at
`third_party/licenses/oboe-LICENSE.txt`. The root `MODULE_LICENSE_APACHE2` is an
empty Android build marker. The application and sample subtree license files
do not apply to the Oboe library linked by MoL Keyboard. No source patch is
applied.

The lock manifest, license snapshot, archive checksum, third-party notice, and
SPDX 2.3 package record are checked by CTest.

## Enabled surface

Only the Oboe library is fetched into Android native builds. The repository's
sample applications, test applications, and tools are not added. Device data
conversion remains enabled so negotiated output formats can be handled without
silently losing compatibility. Oboe selects AAudio on supported Android API
levels and retains its OpenSL ES compatibility fallback; MoL Keyboard does not
implement a new direct OpenSL ES host.
