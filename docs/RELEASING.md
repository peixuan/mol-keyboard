# Releasing

## Release rule

The repository version is currently 0.1.0. Do not create, push, publish, sign,
or describe a `v1.0.0` release until every item in `CODEX_GOAL.md` Definition of
Done is backed by current evidence. Implementation-complete, source-present,
emulator-tested, and build-verified are not substitutes for required physical
platform acceptance.

## Version consistency

For a candidate, update the same semantic version in:

- the root CMake project and generated native version string;
- `apps/web/package.json` and its lockfile;
- Android `versionName`/monotonic `versionCode`;
- iOS short version/monotonic bundle version through CMake;
- HarmonyOS application and entry-package versions;
- the SPDX document namespace and first-party package version;
- package audit expectations, CHANGELOG, and release notes.

Do not change the public C API major merely to match a product release. Follow
`docs/api/ABI_POLICY.md` for ABI evolution.

## Candidate preparation

1. Begin from a clean checkout of the exact candidate commit with no ignored
   SDK, dependency, or build directory copied into it.
2. Re-run the dependency license audit, verify every lock/hash against its
   authoritative source, and update the SPDX SBOM and notices.
3. Run MSVC, GCC, Clang, Emscripten, coverage, static-analysis, sanitizer/fuzz,
   ThreadSanitizer, optimized endurance, and browser matrices.
4. Build the Android, iOS, HarmonyOS, ESP32, and ESP32-S3 products with their
   pinned official toolchains.
5. Run the physical device, input, route, background, 30-minute, I2S capture,
   A2DP, and latency procedures in `docs/testing/TEST_PLAN.md`.
6. Update the platform matrix, benchmark/size report, implementation status,
   limitations, CHANGELOG, and bilingual release notes with actual results.

Any failure or unavailable mandatory environment stops the release. Fixes
restart the affected gate and any dependent long-run measurement on the new
commit.

## Portable native and Web package

Build Web assets before configuring the package preset:

```sh
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
npm --prefix apps/web ci
npm --prefix apps/web test
npm --prefix apps/web run build
cmake --preset package-release
cmake --build --preset package-release
cpack --config build/package-release/CPackConfig.cmake -B build/packages
```

Run `tools/release_size_gate.py` against the optimized core, daemon, CLI, Wasm,
and Web distribution. Run `tools/package_audit.py` against each CPack archive.
The audit requires its CPack `.sha256`, all product/legal/SDK files, exported
`mol::core`, the expected daemon version, and a working CLI help path.

The ZIP/TGZ files are unsigned portable distributions. Do not call them signed
installers. Signing certificates, store credentials, and notarization profiles
remain outside the repository and CI logs.

## Platform artifacts

Archive the unsigned Android release APK and its mapping/manifest evidence; sign
only through the authorized release environment. Apple device archives require
the approved bundle ID, team, entitlements, privacy manifest, signing identity,
and notarization/store workflow. Harmony HAP signing follows the equivalent
external profile. ESP firmware evidence includes bootloader, partition table,
application image, merged flash instructions, map/size output, and HIL report.

Record a cryptographic hash for every distributed artifact. Store public
checksums and licenses beside downloads, but keep raw hardware logs free of
credentials and personal/device identifiers.

## Final review and tag

The release reviewer compares every platform statement to the evidence matrix,
confirms that known limitations are user-visible, installs each distribution
on a clean target, and renders/plays the included example. The candidate commit
must be clean and all referenced reports must identify that commit.

Only then move `[Unreleased]` entries into `[1.0.0]` with the release date,
remove the draft warning from the bilingual release notes, create the annotated
or signed `v1.0.0` tag, and publish exactly the audited artifacts and checksums.
If a published artifact differs by one byte, generate and audit a new candidate;
never silently replace a release file.
