# Quality Gates

| Gate | State | Evidence |
|---|---|---|
| M0 engineering baseline | in progress | Native lifecycle build pending |
| M1 portable sound path | not started | Depends on M0 |
| M2 music semantics | not started | Depends on M1 |
| M3 instruments and DSP | not started | Depends on M2 |
| M4 recording and tools | not started | Depends on M3 |
| M5 desktop headless product | not started | Depends on M4 |
| M6 Web/PWA | not started | Depends on portable core gates |
| M7 Android and iOS | not started | Depends on portable core gates |
| M8 HarmonyOS | not started | Depends on portable core gates |
| M9 ESP32 product | not started | Depends on portable core gates |
| M10 v1.0.0 release | not started | All earlier gates required |

The status `build-verified`, `runtime-verified`, or `device-verified` is used only
after the corresponding real check succeeds.
