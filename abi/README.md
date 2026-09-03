# ABI Baseline

`mol_core-1.0.dump` is the ABI Dumper baseline for the public Linux x86_64 C
ABI. It records exported function signatures and every reachable public type
layout. `mol_core-1.0.symbols` is the exact public export allow-list shared by
the ELF, Mach-O, and PE builds.

The `shared-core` CI job builds `mol_core` with debug information, generates a
fresh dump, checks the exact symbol surface, and runs ABI Compliance Checker
against this baseline. A compatible API addition requires an intentional
allow-list and baseline update. An incompatible change requires a new major API
version and a new baseline; never replace this v1 dump to conceal a reported
break.

Generate and compare the current Linux baseline with:

```sh
cmake --preset ci-shared
cmake --build --preset ci-shared
nm --dynamic --defined-only --format=posix \
  build/ci-shared/libmol_core.so.0.1.0 \
  | cut -d ' ' -f 1 | LC_ALL=C sort > build/mol_core-current.symbols
abi-dumper build/ci-shared/libmol_core.so.0.1.0 \
  -lver current -o build/mol_core-current.dump
abi-compliance-checker -l mol_core \
  -old abi/mol_core-1.0.dump -new build/mol_core-current.dump \
  -report-path build/mol_core-abi-report.html
```
