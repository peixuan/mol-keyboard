# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_workflow "${MOL_SOURCE_DIR}/.github/workflows/ci.yml")
set(_presets "${MOL_SOURCE_DIR}/CMakePresets.json")
set(_desktop_project "${MOL_SOURCE_DIR}/apps/mol-keyboard/CMakeLists.txt")
set(_package_audit "${MOL_SOURCE_DIR}/tools/package_audit.py")
foreach(_required "${_workflow}" "${_presets}" "${_desktop_project}" "${_package_audit}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "macOS desktop acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_workflow}" _workflow_text)
string(FIND "${_workflow_text}" "  macos-desktop:" _job_start)
string(FIND "${_workflow_text}" "  resource-profiles:" _job_end)
if(_job_start EQUAL -1 OR _job_end EQUAL -1 OR _job_end LESS_EQUAL _job_start)
  message(FATAL_ERROR "dedicated macOS desktop CI job is missing")
endif()
math(EXPR _job_length "${_job_end} - ${_job_start}")
string(SUBSTRING "${_workflow_text}" ${_job_start} ${_job_length} _job_text)
foreach(_token
        "runner: [macos-15, macos-15-intel]"
        [[runs-on: ${{ matrix.runner }}]]
        "cmake --preset wasm-release"
        "npm --prefix apps/web run build"
        "cmake --preset ci-macos-desktop"
        "cmake --build --preset ci-macos-desktop"
        "ctest --preset ci-macos-desktop"
        "mol_(desktop_(web_server|app_support|webview)|native_debug)"
        "cpack --config build/ci-macos-desktop/CPackConfig.cmake"
        "shopt -s nullglob"
        "tools/package_audit.py"
        "--expected-version 0.1.0")
  string(FIND "${_job_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS desktop CI job is missing ${_token}")
  endif()
endforeach()

file(READ "${_presets}" _preset_text)
foreach(_token
        "\"name\": \"ci-macos-desktop\""
        "\"MOL_BUILD_DESKTOP_GUI\": \"ON\""
        "\"MOL_BUILD_NATIVE_DEBUG_GUI\": \"ON\""
        "\"MOL_BUILD_TESTS\": \"ON\""
        "\"MOL_PACKAGE_WEB_ASSETS\": \"ON\"")
  string(FIND "${_preset_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS desktop preset is missing ${_token}")
  endif()
endforeach()

file(READ "${_desktop_project}" _desktop_text)
foreach(_token
        "MACOSX_BUNDLE"
        "wx::webview"
        "mol-keyboard.app/Contents/Resources/web"
        "mol_desktop_webview_acceptance"
        "mol_native_debug_gui_acceptance")
  string(FIND "${_desktop_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS desktop project is missing ${_token}")
  endif()
endforeach()

file(READ "${_package_audit}" _audit_text)
foreach(_token
        "sys.platform == \"darwin\""
        "mol-keyboard.app/Contents/MacOS/mol-keyboard"
        "mol-keyboard.app/Contents/Resources/web/index.html"
        "mol-keyboard-debug.app/Contents/MacOS/mol-keyboard-debug"
        "run_native_debug_gui_smoke"
        "run_headless_runtime_smoke")
  string(FIND "${_audit_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS package audit is missing ${_token}")
  endif()
endforeach()
