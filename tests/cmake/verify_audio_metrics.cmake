# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_EXECUTABLE OR NOT DEFINED MOL_EXPECTED)
  message(FATAL_ERROR "MOL_EXECUTABLE and MOL_EXPECTED are required")
endif()

if(DEFINED MOL_EMULATOR AND NOT MOL_EMULATOR STREQUAL "")
  execute_process(
    COMMAND ${MOL_EMULATOR} "${MOL_EXECUTABLE}"
    RESULT_VARIABLE metrics_result
    OUTPUT_VARIABLE metrics_output
    ERROR_VARIABLE metrics_error)
else()
  execute_process(
    COMMAND "${MOL_EXECUTABLE}"
    RESULT_VARIABLE metrics_result
    OUTPUT_VARIABLE metrics_output
    ERROR_VARIABLE metrics_error)
endif()

if(NOT metrics_result EQUAL 0)
  message(FATAL_ERROR "Audio metrics failed (${metrics_result}): ${metrics_error}")
endif()

file(STRINGS "${MOL_EXPECTED}" expected_lines REGEX "^[0-9]+ ")
string(REPLACE "\r\n" "\n" metrics_output "${metrics_output}")
string(REPLACE "\r" "\n" metrics_output "${metrics_output}")
string(REGEX MATCHALL "[^\n]+" actual_lines "${metrics_output}")
list(LENGTH expected_lines expected_count)
list(LENGTH actual_lines actual_count)
if(NOT expected_count EQUAL actual_count)
  message(FATAL_ERROR "Audio metric count mismatch: expected ${expected_count}, actual ${actual_count}")
endif()

set(metric_tolerances 0 0 1500 300 20 300 1500 3 20 10 10 10 10)
math(EXPR last_line "${expected_count} - 1")
foreach(line_index RANGE ${last_line})
  list(GET expected_lines ${line_index} expected_line)
  list(GET actual_lines ${line_index} actual_line)
  string(REGEX REPLACE " +" ";" expected_fields "${expected_line}")
  string(REGEX REPLACE " +" ";" actual_fields "${actual_line}")
  list(LENGTH expected_fields expected_field_count)
  list(LENGTH actual_fields actual_field_count)
  if(NOT expected_field_count EQUAL 13 OR NOT actual_field_count EQUAL 13)
    message(FATAL_ERROR "Malformed audio metric line ${line_index}")
  endif()
  foreach(field_index RANGE 12)
    list(GET expected_fields ${field_index} expected_value)
    list(GET actual_fields ${field_index} actual_value)
    list(GET metric_tolerances ${field_index} tolerance)
    if(field_index LESS 2)
      if(NOT expected_value STREQUAL actual_value)
        message(FATAL_ERROR
                "Audio identity mismatch at line ${line_index}: expected ${expected_value}, actual ${actual_value}")
      endif()
    else()
      math(EXPR difference "${actual_value} - ${expected_value}")
      if(difference LESS 0)
        math(EXPR difference "-${difference}")
      endif()
      if(difference GREATER tolerance)
        message(FATAL_ERROR
                "Audio metric mismatch for ${actual_fields} field ${field_index}: expected ${expected_value} +/- ${tolerance}, actual ${actual_value}")
      endif()
    endif()
  endforeach()
endforeach()

message(STATUS "Audio metrics match ${actual_count} preset golden records")
