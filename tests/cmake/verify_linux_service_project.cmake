# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_runner "${MOL_SOURCE_DIR}/platforms/linux/run-systemd-user-smoke.sh")
set(_registration "${MOL_SOURCE_DIR}/apps/molctl/CMakeLists.txt")
set(_root_registration "${MOL_SOURCE_DIR}/CMakeLists.txt")
set(_test_registration "${MOL_SOURCE_DIR}/tests/CMakeLists.txt")
set(_unit "${MOL_SOURCE_DIR}/packaging/systemd/mol-keyboardd.service")
foreach(_required
        "${_runner}"
        "${_registration}"
        "${_root_registration}"
        "${_test_registration}"
        "${_unit}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "Linux service acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_runner}" _runner_text)
foreach(_token
        "systemd-analyze --user verify"
        "systemctl --user --runtime link"
        "systemctl --user start"
        "--null-backend"
        "record start"
        "diagnostics.benchmark"
        "ExecMainStatus=0"
        "MOL_LINUX_SYSTEMD_USER_SMOKE_PASS")
  string(FIND "${_runner_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Linux systemd acceptance runner is missing ${_token}")
  endif()
endforeach()

file(READ "${_registration}" _registration_text)
foreach(_token
        "mol_macos_launchd_service_simulation"
        "mol_linux_systemd_user_service_smoke"
        "run-systemd-user-smoke.sh"
        "SKIP_RETURN_CODE 77")
  string(FIND "${_registration_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Linux systemd CTest registration is missing ${_token}")
  endif()
endforeach()
string(FIND "${_registration_text}" "find_package(Python3 QUIET" _optional_python)
if(NOT _optional_python EQUAL -1)
  message(FATAL_ERROR "Linux/macOS service acceptance must not make Python optional")
endif()

file(READ "${_root_registration}" _root_registration_text)
string(FIND
       "${_root_registration_text}"
       "find_package(Python3 REQUIRED COMPONENTS Interpreter)"
       _required_python)
if(_required_python EQUAL -1)
  message(FATAL_ERROR "Native test configuration must require Python")
endif()

file(READ "${_test_registration}" _test_registration_text)
foreach(_token "mol_esp32_hil_parser_tests" "mol_esp32_qemu_parser_tests")
  string(FIND "${_test_registration_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Parser test registration is missing ${_token}")
  endif()
endforeach()
string(FIND "${_test_registration_text}" "Python3_Interpreter_FOUND" _optional_parsers)
if(NOT _optional_parsers EQUAL -1)
  message(FATAL_ERROR "Parser tests must not be conditional on optional Python")
endif()

file(READ "${_unit}" _unit_text)
foreach(_token
        "ExecStart=%h/.local/bin/mol-keyboardd"
        "Restart=on-failure"
        "NoNewPrivileges=true"
        "PrivateTmp=true"
        "ProtectSystem=strict"
        "ProtectHome=read-only"
        "ReadWritePaths=%h/.local/state/mol-keyboard"
        "RestrictAddressFamilies=AF_UNIX"
        "WantedBy=default.target")
  string(FIND "${_unit_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Linux systemd service template is missing ${_token}")
  endif()
endforeach()
