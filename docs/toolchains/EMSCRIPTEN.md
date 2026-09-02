# Emscripten Toolchain

The first verified WebAssembly toolchain is Emscripten SDK 6.0.5. It was selected
from the SDK's official `latest` alias on 2026-09-02 and is pinned as follows:

| Item | Pin |
|---|---|
| SDK repository | `https://github.com/emscripten-core/emsdk.git` |
| SDK tag commit | `dfb9d1a46c3bb8f52e1e6324be23123b9d73c190` |
| Emscripten releases commit | `dbd755b5da399329c2576f6e3dfa7f419f5d8409` |
| `emcc --version` revision | `1db513782be24469589d7cb8a1f1834e9a33f271` |
| Bundled Node.js | 22.16.0 |

Emscripten is a build-only dependency and is not committed or packaged with MoL
Keyboard. Its upstream project is dual-licensed under MIT or the University of
Illinois/NCSA Open Source License. The SDK retains the licenses of its bundled
LLVM, Binaryen, Node.js, Python, and system-library components in the local SDK.

## Reproducible local setup

```powershell
git clone --depth 1 --branch 6.0.5 https://github.com/emscripten-core/emsdk.git .cache/emsdk
.cache/emsdk/emsdk.bat install 6.0.5
.cache/emsdk/emsdk.bat activate 6.0.5
.cache/emsdk/emsdk_env.bat
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
```

The repository ignores `.cache/`; no local SDK archive, generated cache, or
absolute machine path enters source control.
