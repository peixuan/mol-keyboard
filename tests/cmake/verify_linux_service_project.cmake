# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_runner "${MOL_SOURCE_DIR}/platforms/linux/run-systemd-user-smoke.sh")
set(_registration "${MOL_SOURCE_DIR}/apps/molctl/CMakeLists.txt")
set(_unit "${MOL_SOURCE_DIR}/packaging/systemd/mol-keyboardd.service")
foreach(_required "${_runner}" "${_registration}" "${_unit}")
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
        "mol_linux_systemd_user_service_smoke"
        "run-systemd-user-smoke.sh"
        "SKIP_RETURN_CODE 77")
  string(FIND "${_registration_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Linux systemd CTest registration is missing ${_token}")
  endif()
endforeach()

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
