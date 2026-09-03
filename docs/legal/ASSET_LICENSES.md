# Asset Licenses and Provenance

## Policy

MoL Keyboard is a clean-room project. Product assets must have a known author,
source, license, and redistribution right before entering the repository. An
asset without that record is rejected. Downloaded fonts, samples, photos, logos,
and recordings are not accepted based only on availability or attribution.

The current product contains no third-party samples, fonts, photos, or musical
recordings. Its 18 baseline instruments are procedural Patch data and do not
embed sampled performances.

## First-party visual identity

The keyboard mark and its raster/vector platform adaptations were created for
this clean-room repository and are copyright 2026 MoL Keyboard contributors,
licensed under Apache-2.0 with the first-party project:

| Asset | Purpose | SHA-256 |
| --- | --- | --- |
| `apps/web/public/icons/mol-keyboard.svg` | Web/PWA icon | `faaa5dbccb23d32ad45eb0d10023c745bc269a61625e56d0b7a1439239bd91e1` |
| `apps/web/public/icons/mol-keyboard-maskable.svg` | maskable PWA icon | `e2ce4df654c3271cf5cc742a0de61f180ea1cc0aafb9e3833aecaece35d90780` |
| `platforms/android/app/src/main/res/drawable/ic_mol_keyboard.xml` | Android vector icon | `b288f26e27f5bc2bd366737e5bc8c70c7fda29d9c77d0485b7fdc1ce593a81a4` |
| `platforms/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png` | Apple application icon | `f6d471ae969d85cee1ba449461a909e51a031396f431e1ed80ac147bb89e99f9` |

The identical 1024-pixel PNG hash is also used for the three HarmonyOS
`app_icon.png`/`startIcon.png` resources. Platform manifests and asset-catalog
JSON are first-party source under Apache-2.0.

## Music and test assets

`examples/sequences/scale-study.molseq` and its JSON source are an original
example composed for this project and licensed under Apache-2.0, as recorded in
`examples/sequences/LICENSE.md`. Built-in `.molpatch.json` files and their
deterministically generated binary/C forms are first-party configuration data
under Apache-2.0.

Files under `tests/golden` and parser fixtures are test data derived from the
first-party engine, formats, and examples. They are not bundled third-party
media and use the repository license.

## Third-party code is not an asset

Dependency notices and preserved license snapshots live in
`THIRD_PARTY_NOTICES.md`, `third_party/licenses`, and the SPDX SBOM. Those files
cover code and build/runtime dependencies, not visual or musical asset rights.

## Adding an asset

Before committing a new asset, record its original URL or first-party author,
exact version/hash, copyright, SPDX-compatible license, modifications, and
where its license text will be packaged. Confirm that the license permits the
intended source and binary distribution. Update this inventory, notices when
required, and the SBOM. Add a test that the packaged product contains both the
asset and its notice. Never commit a credential, private recording, device log,
or personal data as an asset.
