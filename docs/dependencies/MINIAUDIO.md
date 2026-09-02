# miniaudio Dependency Audit

The desktop audio host uses miniaudio 0.11.25 from the official
`mackron/miniaudio` repository at commit
`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`. CMake downloads the commit
archive and requires SHA-256
`1a3a79b80fc6f0b0cc155e28b954a598e0ddfa2db64e2afa8466be88c476fa55`.

## License review

The release offers a choice of public domain/Unlicense or MIT No Attribution.
MoL Keyboard elects MIT-0. The top-level upstream `LICENSE` was inspected
directly from the pinned archive and is preserved with a normalized final
newline at `third_party/licenses/miniaudio-LICENSE.txt`. The release archive
contains no populated Git submodules and no additional license files. The
amalgamated device layer includes namespaced c89atomic support under miniaudio's
published license terms.

No source patch is applied. The lock manifest, license snapshot, and SPDX 2.3
SBOM are checked by CTest.

## Enabled surface

Only low-level playback-device I/O is enabled:

- WASAPI plus the null test backend on Windows;
- Core Audio plus null on macOS;
- PulseAudio, ALSA, JACK, and null on Linux.

Decoders, encoders, WAV/FLAC/MP3 code paths, the resource manager, node graph,
high-level engine, generators, examples, tests, tools, extra nodes, libvorbis,
and libopus integration are disabled. Runtime loading remains enabled on Linux
so development packages for every backend are not mandatory.
