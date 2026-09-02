# SPDX-License-Identifier: Apache-2.0

execute_process(COMMAND "${MOL_SEQ}" binary-to-json "${INPUT}" "${OUTPUT}"
                RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Cannot inspect imported MIDI sequence: ${error}")
endif()
file(READ "${OUTPUT}" json)
string(REGEX MATCHALL "\"type\": \"note_on\"" note_ons "${json}")
string(REGEX MATCHALL "\"type\": \"note_off\"" note_offs "${json}")
list(LENGTH note_ons note_on_count)
list(LENGTH note_offs note_off_count)
if(NOT note_on_count EQUAL 4 OR NOT note_off_count EQUAL 4)
  message(FATAL_ERROR "MIDI note semantics changed: ${note_on_count} on / ${note_off_count} off")
endif()
if(NOT json MATCHES "\"type\": \"set_tempo\"")
  message(FATAL_ERROR "Imported MIDI tempo event is missing")
endif()
message(STATUS "MIDI import preserved four note gestures and tempo")
