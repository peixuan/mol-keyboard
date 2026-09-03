# SPDX-License-Identifier: Apache-2.0

add_executable(mol_core_wasm wasm_exports.c)
target_link_libraries(mol_core_wasm PRIVATE mol::core)
set_target_properties(
  mol_core_wasm
  PROPERTIES C_EXTENSIONS OFF
             C_STANDARD_REQUIRED ON
             OUTPUT_NAME mol_core
             SUFFIX .mjs)
mol_enable_warnings(mol_core_wasm)
target_link_options(
  mol_core_wasm
  PRIVATE --no-entry
          -sWASM=1
          -sMODULARIZE=1
          -sEXPORT_ES6=1
          -sENVIRONMENT=web,worker,node
          -sALLOW_MEMORY_GROWTH=0
          -sINITIAL_MEMORY=16777216
          "-sEXPORTED_FUNCTIONS=['_mol_wasm_initialize','_mol_wasm_note_on','_mol_wasm_note_off','_mol_wasm_submit_scalar','_mol_wasm_submit_integer','_mol_wasm_submit_parameter','_mol_wasm_submit_preset','_mol_wasm_submit_scale','_mol_wasm_submit_arpeggiator','_mol_wasm_submit_time_signature','_mol_wasm_submit_metronome','_mol_wasm_submit_portamento','_mol_wasm_submit_action','_mol_wasm_poll_events','_mol_wasm_event_buffer','_mol_wasm_export_recording','_mol_wasm_recording_last_error','_mol_wasm_recording_buffer','_mol_wasm_sequence_input_buffer','_mol_wasm_sequence_input_capacity','_mol_wasm_load_sequence','_mol_wasm_render']"
          "-sEXPORTED_RUNTIME_METHODS=['cwrap','HEAPF32','HEAPU8','HEAPU32']")

add_executable(mol_audio_worklet_core wasm_exports.c)
target_link_libraries(mol_audio_worklet_core PRIVATE mol::core)
set_target_properties(
  mol_audio_worklet_core
  PROPERTIES C_EXTENSIONS OFF
             C_STANDARD_REQUIRED ON
             OUTPUT_NAME mol_audio_worklet_core
             SUFFIX .js)
mol_enable_warnings(mol_audio_worklet_core)
set_property(
  TARGET mol_audio_worklet_core
  APPEND
  PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mol_audio_worklet.mjs)
target_link_options(
  mol_audio_worklet_core
  PRIVATE --no-entry
          -sWASM=1
          -sMODULARIZE=1
          -sEXPORT_ES6=1
          -sENVIRONMENT=worklet
          -sWASM_ASYNC_COMPILATION=0
          -sASSERTIONS=0
          -sALLOW_MEMORY_GROWTH=0
          -sINITIAL_MEMORY=16777216
          "--extern-post-js=${CMAKE_CURRENT_SOURCE_DIR}/mol_audio_worklet.mjs"
          "-sEXPORTED_FUNCTIONS=['_mol_wasm_initialize','_mol_wasm_note_on','_mol_wasm_note_off','_mol_wasm_submit_scalar','_mol_wasm_submit_integer','_mol_wasm_submit_parameter','_mol_wasm_submit_preset','_mol_wasm_submit_scale','_mol_wasm_submit_arpeggiator','_mol_wasm_submit_time_signature','_mol_wasm_submit_metronome','_mol_wasm_submit_portamento','_mol_wasm_submit_action','_mol_wasm_poll_events','_mol_wasm_event_buffer','_mol_wasm_export_recording','_mol_wasm_recording_last_error','_mol_wasm_recording_buffer','_mol_wasm_sequence_input_buffer','_mol_wasm_sequence_input_capacity','_mol_wasm_load_sequence','_mol_wasm_render']"
          "-sEXPORTED_RUNTIME_METHODS=['cwrap','HEAPF32','HEAPU8','HEAPU32']")

configure_file(test_audio_worklet_browser.html test_audio_worklet_browser.html COPYONLY)

if(MOL_BUILD_TESTS)
  if(DEFINED ENV{EMSDK_NODE})
    set(MOL_NODE_EXECUTABLE "$ENV{EMSDK_NODE}")
  else()
    find_program(MOL_NODE_EXECUTABLE node REQUIRED NO_CMAKE_FIND_ROOT_PATH)
  endif()
  add_test(
    NAME mol_wasm_c4_conformance
    COMMAND ${MOL_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_wasm.mjs
            $<TARGET_FILE:mol_core_wasm>)
  add_test(
    NAME mol_audio_worklet_conformance
    COMMAND ${MOL_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_audio_worklet.mjs
            $<TARGET_FILE:mol_audio_worklet_core>
            $<TARGET_FILE_DIR:mol_audio_worklet_core>/mol_audio_worklet_core.wasm)
endif()
