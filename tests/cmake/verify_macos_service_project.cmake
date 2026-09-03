# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_runner "${MOL_SOURCE_DIR}/platforms/macos/run-launchd-smoke.sh")
set(_simulation "${MOL_SOURCE_DIR}/tests/integration/test_macos_launchd_sim.py")
set(_registration "${MOL_SOURCE_DIR}/apps/molctl/CMakeLists.txt")
set(_plist
    "${MOL_SOURCE_DIR}/packaging/launchd/cn.zhangpeixuan.molkeyboard.daemon.plist")
set(_workflow "${MOL_SOURCE_DIR}/.github/workflows/ci.yml")
foreach(_required "${_runner}" "${_simulation}" "${_registration}" "${_plist}" "${_workflow}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "macOS service acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_simulation}" _simulation_text)
foreach(_token
        "MOL_LAUNCHD_SIM_DAEMON"
        "launchctl bootstrap"
        "launchctl bootout"
        "MOL_MACOS_LAUNCHD_SMOKE_PASS"
        "MOL_MACOS_LAUNCHD_SIMULATION_PASS")
  string(FIND "${_simulation_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS launchd simulation is missing ${_token}")
  endif()
endforeach()

file(READ "${_runner}" _runner_text)
foreach(_token
        "launchctl bootstrap"
        "launchctl bootout"
        "plutil -lint"
        "--null-backend"
        "preset set violin"
        "record start"
        "note on 60"
        "playback.stop"
        "diagnostics.benchmark"
        "last exit code = 0"
        "MOL_MACOS_LAUNCHD_SMOKE_PASS")
  string(FIND "${_runner_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS launchd acceptance runner is missing ${_token}")
  endif()
endforeach()

file(READ "${_registration}" _registration_text)
foreach(_token
        "if(APPLE)"
        "mol_macos_launchd_service_smoke"
        "run-launchd-smoke.sh"
        "NOT CMAKE_CROSSCOMPILING"
        "mol_macos_launchd_service_simulation"
        "test_macos_launchd_sim.py")
  string(FIND "${_registration_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS launchd CTest registration is missing ${_token}")
  endif()
endforeach()

file(READ "${_plist}" _plist_text)
foreach(_token
        "cn.zhangpeixuan.molkeyboard.daemon"
        "/usr/local/bin/mol-keyboardd"
        "RunAtLoad"
        "KeepAlive"
        "SuccessfulExit")
  string(FIND "${_plist_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS launchd template is missing ${_token}")
  endif()
endforeach()

file(READ "${_workflow}" _workflow_text)
foreach(_token "macos-15" "ctest --preset")
  string(FIND "${_workflow_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "macOS CI execution path is missing ${_token}")
  endif()
endforeach()
