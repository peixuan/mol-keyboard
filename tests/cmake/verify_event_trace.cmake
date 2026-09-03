# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_EXECUTABLE OR NOT DEFINED MOL_EXPECTED)
  message(FATAL_ERROR "MOL_EXECUTABLE and MOL_EXPECTED are required")
endif()

if(DEFINED MOL_EMULATOR AND NOT MOL_EMULATOR STREQUAL "")
  execute_process(
    COMMAND ${MOL_EMULATOR} "${MOL_EXECUTABLE}"
    RESULT_VARIABLE trace_result
    OUTPUT_VARIABLE trace_output
    ERROR_VARIABLE trace_error)
else()
  execute_process(
    COMMAND "${MOL_EXECUTABLE}"
    RESULT_VARIABLE trace_result
    OUTPUT_VARIABLE trace_output
    ERROR_VARIABLE trace_error)
endif()

if(NOT trace_result EQUAL 0)
  message(FATAL_ERROR "Event trace failed (${trace_result}): ${trace_error}")
endif()

file(READ "${MOL_EXPECTED}" expected_output)
string(STRIP "${trace_output}" trace_output)
string(STRIP "${expected_output}" expected_output)
if(NOT trace_output STREQUAL expected_output)
  message(FATAL_ERROR
          "Event trace mismatch\nexpected: ${expected_output}\nactual:   ${trace_output}")
endif()

message(STATUS "Event trace matches: ${trace_output}")
