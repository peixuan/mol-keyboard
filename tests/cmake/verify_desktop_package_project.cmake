# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_presets "${MOL_SOURCE_DIR}/CMakePresets.json")
set(_package_audit "${MOL_SOURCE_DIR}/tools/package_audit.py")
foreach(_required "${_presets}" "${_package_audit}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "desktop package acceptance input is missing: ${_required}")
  endif()
endforeach()

file(READ "${_presets}" _preset_json)
string(JSON _preset_count LENGTH "${_preset_json}" configurePresets)
set(_package_preset_index -1)
math(EXPR _preset_last "${_preset_count} - 1")
foreach(_index RANGE 0 ${_preset_last})
  string(JSON _name GET "${_preset_json}" configurePresets ${_index} name)
  if(_name STREQUAL "package-release")
    set(_package_preset_index ${_index})
    break()
  endif()
endforeach()
if(_package_preset_index EQUAL -1)
  message(FATAL_ERROR "package-release configure preset is missing")
endif()

foreach(_option MOL_BUILD_DESKTOP_GUI MOL_BUILD_NATIVE_DEBUG_GUI MOL_PACKAGE_WEB_ASSETS)
  string(JSON _value GET "${_preset_json}" configurePresets ${_package_preset_index}
              cacheVariables ${_option})
  if(NOT _value STREQUAL "ON")
    message(FATAL_ERROR "package-release must set ${_option}=ON")
  endif()
endforeach()

file(READ "${_package_audit}" _audit_text)
foreach(_token
        "mol-keyboard-debug.exe"
        "mol-keyboard-debug.app/Contents/MacOS/mol-keyboard-debug"
        "bin/mol-keyboard-debug"
        "run_native_debug_gui_smoke"
        "native_debug_gui_smoke")
  string(FIND "${_audit_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "desktop package audit is missing ${_token}")
  endif()
endforeach()
