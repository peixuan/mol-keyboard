# SPDX-License-Identifier: Apache-2.0

file(WRITE "${INPUT}" "MThd")
execute_process(COMMAND "${MOL_SEQ}" midi-import "${INPUT}" "${OUTPUT}"
                RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0)
  message(FATAL_ERROR "Truncated MIDI unexpectedly imported")
endif()
if(EXISTS "${OUTPUT}")
  file(REMOVE "${OUTPUT}")
endif()
message(STATUS "Truncated MIDI failed safely: ${error}")
