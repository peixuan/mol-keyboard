# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MOL_PATCHC OR NOT DEFINED MOL_PATCH_DIR OR NOT DEFINED MOL_OUTPUT_DIR)
  message(FATAL_ERROR "MOL_PATCHC, MOL_PATCH_DIR, and MOL_OUTPUT_DIR are required")
endif()

set(preset_ids
    grand-piano
    electric-piano
    harpsichord
    church-organ
    jazz-organ
    nylon-guitar
    steel-guitar
    violin
    cello
    flute
    clarinet
    synth-lead
    synth-pad
    synth-bass
    choir
    vibraphone
    harp
    music-box)

file(MAKE_DIRECTORY "${MOL_OUTPUT_DIR}")
foreach(preset_id IN LISTS preset_ids)
  set(source "${MOL_PATCH_DIR}/${preset_id}.molpatch.json")
  set(first "${MOL_OUTPUT_DIR}/${preset_id}-first.molpatch")
  set(second "${MOL_OUTPUT_DIR}/${preset_id}-second.molpatch")
  execute_process(
    COMMAND "${MOL_PATCHC}" "${source}" "${first}"
    RESULT_VARIABLE first_result
    OUTPUT_VARIABLE first_output
    ERROR_VARIABLE first_error)
  execute_process(
    COMMAND "${MOL_PATCHC}" "${source}" "${second}"
    RESULT_VARIABLE second_result
    OUTPUT_VARIABLE second_output
    ERROR_VARIABLE second_error)
  if(NOT first_result EQUAL 0 OR NOT second_result EQUAL 0)
    message(FATAL_ERROR
            "mol-patchc failed for ${preset_id}: ${first_error}${second_error}")
  endif()
  file(SIZE "${first}" first_size)
  file(SIZE "${second}" second_size)
  file(SHA256 "${first}" first_hash)
  file(SHA256 "${second}" second_hash)
  if(NOT first_size EQUAL 120 OR NOT second_size EQUAL 120 OR
     NOT first_hash STREQUAL second_hash)
    message(FATAL_ERROR
            "Non-deterministic ${preset_id}: ${first_size}/${first_hash} vs ${second_size}/${second_hash}")
  endif()
endforeach()

set(c_first "${MOL_OUTPUT_DIR}/grand-piano-first.c")
set(c_second "${MOL_OUTPUT_DIR}/grand-piano-second.c")
execute_process(
  COMMAND "${MOL_PATCHC}" "${MOL_PATCH_DIR}/grand-piano.molpatch.json"
          "${MOL_OUTPUT_DIR}/grand-piano-c-first.molpatch"
          --c-output "${c_first}" --symbol mol_builtin_grand_piano
  RESULT_VARIABLE c_first_result
  ERROR_VARIABLE c_first_error)
execute_process(
  COMMAND "${MOL_PATCHC}" "${MOL_PATCH_DIR}/grand-piano.molpatch.json"
          "${MOL_OUTPUT_DIR}/grand-piano-c-second.molpatch"
          --c-output "${c_second}" --symbol mol_builtin_grand_piano
  RESULT_VARIABLE c_second_result
  ERROR_VARIABLE c_second_error)
if(NOT c_first_result EQUAL 0 OR NOT c_second_result EQUAL 0)
  message(FATAL_ERROR "C-array generation failed: ${c_first_error}${c_second_error}")
endif()
file(SHA256 "${c_first}" c_first_hash)
file(SHA256 "${c_second}" c_second_hash)
if(NOT c_first_hash STREQUAL c_second_hash)
  message(FATAL_ERROR "Generated C arrays are not byte-identical")
endif()

message(STATUS "All 18 patch sources compiled deterministically")
