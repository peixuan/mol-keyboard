# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_runner "${MOL_SOURCE_DIR}/platforms/windows/run-startup-smoke.ps1")
set(_installer "${MOL_SOURCE_DIR}/packaging/windows/install-user-startup.ps1")
set(_uninstaller "${MOL_SOURCE_DIR}/packaging/windows/uninstall-user-startup.ps1")
set(_registration "${MOL_SOURCE_DIR}/apps/molctl/CMakeLists.txt")
foreach(_required "${_runner}" "${_installer}" "${_uninstaller}" "${_registration}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "Windows service acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_runner}" _runner_text)
foreach(_token
        "Start-Process -FilePath $shortcutPath -WindowStyle Hidden"
        "record', 'start"
        "diagnostics.benchmark"
        "WaitForExit(5000)"
        "MOL_WINDOWS_STARTUP_SMOKE_PASS")
  string(FIND "${_runner_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Windows Startup acceptance runner is missing ${_token}")
  endif()
endforeach()

foreach(_script "${_installer}" "${_uninstaller}")
  file(READ "${_script}" _script_text)
  foreach(_token "StartupDirectory" "MoL Keyboard Service.lnk")
    string(FIND "${_script_text}" "${_token}" _token_offset)
    if(_token_offset EQUAL -1)
      message(FATAL_ERROR "Windows Startup script is missing ${_token}: ${_script}")
    endif()
  endforeach()
endforeach()

file(READ "${_registration}" _registration_text)
foreach(_token
        "if(WIN32)"
        "mol_windows_startup_service_smoke"
        "run-startup-smoke.ps1"
        "RUN_SERIAL TRUE")
  string(FIND "${_registration_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Windows Startup CTest registration is missing ${_token}")
  endif()
endforeach()
