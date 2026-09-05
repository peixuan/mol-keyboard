# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_SOURCE_DIR)
  message(FATAL_ERROR "MOL_SOURCE_DIR is required")
endif()

set(_android "${MOL_SOURCE_DIR}/platforms/android")
set(_activity "${_android}/app/src/main/java/cn/zhangpeixuan/molkeyboard/MainActivity.kt")
set(_instrumentation
    "${_android}/app/src/androidTest/java/cn/zhangpeixuan/molkeyboard/AndroidSmokeInstrumentation.kt")
set(_gate "${MOL_SOURCE_DIR}/tools/android_emulator_gate.py")
set(_workflow "${MOL_SOURCE_DIR}/.github/workflows/ci.yml")
set(_tests "${MOL_SOURCE_DIR}/tests/CMakeLists.txt")

foreach(_required IN ITEMS "${_activity}" "${_instrumentation}" "${_gate}" "${_workflow}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "Required Android product/gate file is missing: ${_required}")
  endif()
endforeach()

file(READ "${_activity}" _activity_text)
file(READ "${_instrumentation}" _instrumentation_text)
file(READ "${_gate}" _gate_text)
file(READ "${_workflow}" _workflow_text)
file(READ "${_tests}" _tests_text)

string(REGEX MATCHALL "KeyEvent[.]KEYCODE_[A-Z0-9_]+ to [0-9]+" _key_mappings
                     "${_activity_text}")
list(LENGTH _key_mappings _key_mapping_count)
if(NOT _key_mapping_count EQUAL 30)
  message(FATAL_ERROR
          "Android production hardware keyboard must map exactly 30 notes; found ${_key_mapping_count}")
endif()

foreach(_token
        IN ITEMS
           "val note = NOTE_BY_KEY_CODE[event.keyCode] ?: return super.dispatchKeyEvent(event)"
           "hardwareKeys"
           "hardwareRepeatSuppressed"
           "waitForKeyboardEvent"
           "EVENT_NOTE_STARTED"
           "EVENT_NOTE_RELEASED")
  string(FIND "${_activity_text}${_instrumentation_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Android hardware-key acceptance is missing ${_token}")
  endif()
endforeach()

foreach(_token
        IN ITEMS
           "INSTRUMENTATION_CODE:"
           "hardwareKeys"
           "hardwareRepeatSuppressed"
           "lockedCallbacks"
           "focusInterrupted"
           "idleBackgroundStopped"
           "schema_version")
  string(FIND "${_gate_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Android emulator fail-closed gate is missing ${_token}")
  endif()
endforeach()

foreach(_token
        IN ITEMS
           "system-images;android-35;google_apis;x86_64"
           "-avd mol_android_ci"
           "tools/android_emulator_gate.py"
           "emu kill")
  string(FIND "${_workflow_text}" "${_token}" _token_offset)
  if(_token_offset EQUAL -1)
    message(FATAL_ERROR "Android emulator CI wiring is missing ${_token}")
  endif()
endforeach()

foreach(_test_name IN ITEMS mol_android_emulator_gate_tests mol_android_project_audit)
  string(FIND "${_tests_text}" "${_test_name}" _test_offset)
  if(_test_offset EQUAL -1)
    message(FATAL_ERROR "Android project test is not registered: ${_test_name}")
  endif()
endforeach()

message(STATUS "Android project and emulator gate audit passed")
