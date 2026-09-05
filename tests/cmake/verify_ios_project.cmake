# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_ios "${MOL_SOURCE_DIR}/platforms/ios")
set(_controller "${_ios}/MOLViewController.mm")
set(_hardware_keys "${_ios}/mol_ios_hardware_keys.c")
set(_runner "${_ios}/run-simulator-smoke.sh")
set(_tests "${MOL_SOURCE_DIR}/tests/CMakeLists.txt")
set(_workflow "${MOL_SOURCE_DIR}/.github/workflows/ci.yml")

foreach(_required
        "${_ios}/CMakeLists.txt"
        "${_ios}/Info.plist.in"
        "${_ios}/PrivacyInfo.xcprivacy"
        "${_ios}/build-app.sh"
        "${_controller}"
        "${_hardware_keys}"
        "${_ios}/mol_ios_hardware_keys.h"
        "${_runner}"
        "${_tests}"
        "${_workflow}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "iOS application or acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_ios}/CMakeLists.txt" _ios_cmake_text)
file(READ "${_tests}" _tests_text)
string(FIND "${_ios_cmake_text}" "TARGET_BUNDLE_DIR:mol_ios_app" _bundle_dir_offset)
if(_bundle_dir_offset EQUAL -1)
  message(FATAL_ERROR "iOS resources are not installed into the flat application bundle")
endif()
string(FIND "${_ios_cmake_text}" "TARGET_BUNDLE_CONTENT_DIR:mol_ios_app" _content_dir_offset)
if(NOT _content_dir_offset EQUAL -1)
  message(FATAL_ERROR "iOS resources use the macOS Contents directory")
endif()
foreach(_text IN ITEMS _ios_cmake_text _tests_text)
  string(FIND "${${_text}}" "mol_ios_hardware_keys.c" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "iOS hardware-key production/test build wiring is missing")
  endif()
endforeach()
string(FIND "${_tests_text}" "mol_ios_hardware_key_tests" _test_offset)
if(_test_offset EQUAL -1)
  message(FATAL_ERROR "iOS hardware-key executable test is not registered")
endif()

file(READ "${_hardware_keys}" _hardware_keys_text)
foreach(_token
        "MOL_IOS_HARDWARE_KEY_CAPACITY"
        "MOL_IOS_HARDWARE_KEY_ACTION_SUSTAIN"
        "MOL_IOS_HARDWARE_GESTURE_PREFIX"
        "mol_ios_hardware_keys_cancel_press"
        "mol_ios_hardware_keys_release_all")
  string(FIND "${_hardware_keys_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "iOS production hardware-key state is missing ${_token}")
  endif()
endforeach()

file(READ "${_controller}" _controller_text)
foreach(_token
        "--mol-simulator-smoke"
        "callAsyncJavaScript"
        "runtime.status"
        [[\"version\":2]]
        "MOL_IOS_SIMULATOR_SMOKE_PASS"
        "MOL_IOS_SIMULATOR_SMOKE_FAIL"
        "mol_ios_hardware_keys_process"
        "mol_ios_hardware_keys_release_all")
  string(FIND "${_controller_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "iOS packaged UI/bridge smoke is missing ${_token}")
  endif()
endforeach()

file(READ "${_runner}" _runner_text)
foreach(_token
        "simctl bootstatus"
        "simctl install"
        "simctl launch"
        "MOL_IOS_SIMULATOR_SMOKE_PASS"
        "MOL_IOS_SIMULATOR_SMOKE_FAIL"
        "screenshot.png")
  string(FIND "${_runner_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "iOS Simulator runner is missing ${_token}")
  endif()
endforeach()

file(READ "${_workflow}" _workflow_text)
string(FIND "${_workflow_text}" "platforms/ios/run-simulator-smoke.sh" _runner_offset)
if(_runner_offset EQUAL -1)
  message(FATAL_ERROR "CI does not execute the iOS Simulator acceptance runner")
endif()
