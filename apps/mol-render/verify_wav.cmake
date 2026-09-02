# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED WAV_PATH OR NOT EXISTS "${WAV_PATH}")
  message(FATAL_ERROR "Expected WAV output does not exist: ${WAV_PATH}")
endif()

file(SIZE "${WAV_PATH}" _mol_wav_size)
if(_mol_wav_size LESS_EQUAL 44)
  message(FATAL_ERROR "WAV output is too small: ${_mol_wav_size} bytes")
endif()

file(READ "${WAV_PATH}" _mol_wav_prefix OFFSET 0 LIMIT 12 HEX)
if(NOT _mol_wav_prefix MATCHES
   "^52494646[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]57415645$")
  message(FATAL_ERROR "WAV output has an invalid RIFF/WAVE header: ${_mol_wav_prefix}")
endif()

file(READ "${WAV_PATH}" _mol_wav_audio OFFSET 44 LIMIT 512 HEX)
if(_mol_wav_audio MATCHES "^0+$")
  message(FATAL_ERROR "WAV output contains only silence in its first audio block")
endif()

message(STATUS "Verified non-silent PCM WAV: ${WAV_PATH} (${_mol_wav_size} bytes)")
