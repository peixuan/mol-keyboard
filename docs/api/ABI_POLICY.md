# C ABI Policy

## Contract

The supported integration boundary is the ISO C11 API under `include/mol`.
Headers are usable from C and C++17. C++ objects, exceptions, templates, STL
containers, platform handles, compiler-specific bit fields, and ownership of
allocated memory never cross this boundary.

`MOL_API_VERSION` packs a 16-bit major and 16-bit minor value. The current API
version is 1.0 (`0x00010000`); the product version remains independent.

## Versioned structures

Every public structure that may evolve begins with `struct_size` and, where it
can cross a stored or process boundary, `api_version`. Callers initialize the
entire object to zero, set these fields, and then populate known members.

Compatible minor releases may append fields. They do not reorder, remove,
reinterpret, or change the width of existing fields. Implementations inspect
`struct_size` before reading appended fields and use deterministic defaults
when an older caller did not provide them. Reserved bytes must be zero when a
format validator requires canonical encoding.

## Types and ownership

- Wire-visible integers have fixed widths from `<stdint.h>`.
- Frame and gesture identifiers are unsigned 64-bit values.
- Public enum-like types use `uint32_t` or `uint16_t` typedefs plus constants.
- UTF-8 strings are immutable pointers with documented lifetime, or buffers
  paired with an explicit capacity.
- Arrays are pointer-plus-count pairs; no flexible array crosses the ABI.
- Engine storage and all output buffers remain owned by the caller.
- Returned preset strings and built-in Patch bytes are immutable static data.

## Compatibility rules

A minor API change may add a function, constant, command, event, or appended
structure field while preserving all existing source and binary behavior.
Unknown optional stored records are skipped only where the format explicitly
allows it. Unknown commands, required records, flags, and versions fail with a
defined `mol_result_t`; they are never guessed.

A major API change is required for an incompatible layout, symbol signature,
ownership rule, semantic reinterpretation, or removal. A major change must ship
under a new `MOL_API_VERSION_MAJOR`, update every serialized bridge, and include
a migration section in release notes.

## Symbol and build policy

The installed CMake package exports `mol::core`. Consumers include
`<mol/mol.h>` and do not include private files under `src`. The project does not
promise ABI compatibility for internal C++ libraries, daemon internals, or
platform host classes. Static and shared builds must expose the same public C
symbols and behavior.

Public declarations carry `MOL_API`. A shared build defines
`MOL_CORE_SHARED`; Windows then uses explicit import/export attributes, while
ELF and Mach-O shared builds use default visibility only for those declarations.
All other symbols are hidden. The authoritative version 1.0 allow-list is
`abi/mol_core-1.0.symbols`; the corresponding Linux x86_64 ABI Dumper baseline
is `abi/mol_core-1.0.dump`. Baseline provenance and update rules are recorded in
`abi/README.md`.

## Verification

CI compiles and runs independent C11 and C++17 consumers in both the ordinary
static configuration and the `ci-shared` configuration. Applicable native
suites additionally install into a fresh isolated prefix, disable CMake package
registries, resolve the installed `mol::core` target, compile every public header
independently as C11 and C++17, and run both consumers against the installed
library. Unit tests check structure and format constants and Native/Wasm suites
exercise shared conformance fixtures. The Linux shared job compares the exported
symbol set exactly against the 47-symbol allow-list, generates a fresh ABI dump,
and requires ABI
Compliance Checker to report binary and source compatibility with the 1.0
baseline. Windows and macOS shared jobs build and execute the public consumers.

The isolated consumer gate verifies the installed `mol::core` import target,
headers, library, and versioned CMake package files. Any ABI-affecting change
requires all of these tests, a reviewed baseline decision, this policy review,
and an explicit changelog entry. A compatible symbol addition updates both
baseline files in the same change; a removal or incompatible signature/layout change
requires a new ABI major and migration notes rather than overwriting history.
