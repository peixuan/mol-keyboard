# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_ios "${MOL_SOURCE_DIR}/platforms/ios")
set(_controller "${_ios}/MOLViewController.mm")
set(_runner "${_ios}/run-simulator-smoke.sh")
set(_workflow "${MOL_SOURCE_DIR}/.github/workflows/ci.yml")

foreach(_required
        "${_ios}/CMakeLists.txt"
        "${_ios}/Info.plist.in"
        "${_ios}/PrivacyInfo.xcprivacy"
        "${_ios}/build-app.sh"
        "${_controller}"
        "${_runner}"
        "${_workflow}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "iOS application or acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_controller}" _controller_text)
foreach(_token
        "--mol-simulator-smoke"
        "callAsyncJavaScript"
        "runtime.status"
        [[\"version\":2]]
        "MOL_IOS_SIMULATOR_SMOKE_PASS"
        "MOL_IOS_SIMULATOR_SMOKE_FAIL")
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
