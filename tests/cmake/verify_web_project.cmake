# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_service_spec "${MOL_SOURCE_DIR}/apps/web/e2e/service-controller.spec.ts")
set(_package "${MOL_SOURCE_DIR}/apps/web/package.json")
set(_workflow "${MOL_SOURCE_DIR}/.github/workflows/ci.yml")

foreach(_required "${_service_spec}" "${_package}" "${_workflow}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "Web acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_service_spec}" _service_spec_text)
foreach(_token
        "MOL_DAEMON"
        "Desktop service executable is required"
        "controls a real authenticated desktop service"
        "system.shutdown")
  string(FIND "${_service_spec_text}" "${_token}" _offset)
  if(_offset EQUAL -1)
    message(FATAL_ERROR "Web service acceptance is missing ${_token}")
  endif()
endforeach()
string(FIND "${_service_spec_text}" "test.skip(!executableExists" _optional_daemon)
if(NOT _optional_daemon EQUAL -1)
  message(FATAL_ERROR "Web service acceptance must not skip a missing daemon")
endif()

file(READ "${_package}" _package_text)
foreach(_token [["test":]] [["build":]] [["test:browser":]])
  string(FIND "${_package_text}" "${_token}" _offset)
  if(_offset EQUAL -1)
    message(FATAL_ERROR "Web package script is missing ${_token}")
  endif()
endforeach()

file(READ "${_workflow}" _workflow_text)
foreach(_token
        "Build native daemon for browser acceptance"
        "node apps/web/scripts/run-browser-tests.mjs install --with-deps chromium firefox webkit"
        [[MOL_DAEMON="$PWD/build/ci-linux-gcc/apps/mol-keyboardd/mol-keyboardd"]]
        "npm --prefix apps/web run test:browser")
  string(FIND "${_workflow_text}" "${_token}" _offset)
  if(_offset EQUAL -1)
    message(FATAL_ERROR "CI Web acceptance wiring is missing ${_token}")
  endif()
endforeach()
