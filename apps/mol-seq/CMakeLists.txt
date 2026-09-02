# SPDX-License-Identifier: Apache-2.0

add_executable(mol-seq main.cpp midi.cpp operations.cpp sequence_document.cpp)
target_link_libraries(mol-seq PRIVATE mol::core mol::json)
set_target_properties(mol-seq PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD 17
                                         CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol-seq)
if(MSVC)
  target_compile_definitions(mol-seq PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS mol-seq RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
  set(_mol_sequence_example "${PROJECT_SOURCE_DIR}/examples/sequences/scale-study.molseq.json")
  set(_mol_sequence_first "${CMAKE_CURRENT_BINARY_DIR}/scale-study-first.molseq")
  set(_mol_sequence_json "${CMAKE_CURRENT_BINARY_DIR}/scale-study-roundtrip.molseq.json")
  set(_mol_sequence_second "${CMAKE_CURRENT_BINARY_DIR}/scale-study-second.molseq")
  set(_mol_sequence_midi "${CMAKE_CURRENT_BINARY_DIR}/scale-study.mid")
  set(_mol_sequence_midi_second "${CMAKE_CURRENT_BINARY_DIR}/scale-study-second.mid")
  set(_mol_sequence_midi_import "${CMAKE_CURRENT_BINARY_DIR}/scale-study-midi-import.molseq")
  set(_mol_sequence_trimmed "${CMAKE_CURRENT_BINARY_DIR}/scale-study-trimmed.molseq")
  set(_mol_sequence_quantized "${CMAKE_CURRENT_BINARY_DIR}/scale-study-quantized.molseq")
  set(_mol_sequence_merged "${CMAKE_CURRENT_BINARY_DIR}/scale-study-merged.molseq")
  add_test(NAME mol_seq_json_to_binary
           COMMAND mol-seq json-to-binary "${_mol_sequence_example}" "${_mol_sequence_first}")
  add_test(NAME mol_seq_validate COMMAND mol-seq validate "${_mol_sequence_first}")
  set_tests_properties(mol_seq_validate PROPERTIES DEPENDS mol_seq_json_to_binary)
  add_test(NAME mol_seq_binary_to_json
           COMMAND mol-seq binary-to-json "${_mol_sequence_first}" "${_mol_sequence_json}")
  set_tests_properties(mol_seq_binary_to_json PROPERTIES DEPENDS mol_seq_json_to_binary)
  add_test(NAME mol_seq_json_roundtrip
           COMMAND mol-seq json-to-binary "${_mol_sequence_json}" "${_mol_sequence_second}")
  set_tests_properties(mol_seq_json_roundtrip PROPERTIES DEPENDS mol_seq_binary_to_json)
  add_test(
    NAME mol_seq_binary_determinism
    COMMAND ${CMAKE_COMMAND} -DFIRST=${_mol_sequence_first} -DSECOND=${_mol_sequence_second}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_roundtrip.cmake)
  set_tests_properties(mol_seq_binary_determinism PROPERTIES DEPENDS mol_seq_json_roundtrip)
  add_test(NAME mol_seq_midi_export
           COMMAND mol-seq midi-export "${_mol_sequence_first}" "${_mol_sequence_midi}")
  set_tests_properties(mol_seq_midi_export PROPERTIES DEPENDS mol_seq_json_to_binary)
  add_test(NAME mol_seq_midi_import
           COMMAND mol-seq midi-import "${_mol_sequence_midi}" "${_mol_sequence_midi_import}")
  set_tests_properties(mol_seq_midi_import PROPERTIES DEPENDS mol_seq_midi_export)
  add_test(NAME mol_seq_midi_import_validate COMMAND mol-seq validate "${_mol_sequence_midi_import}")
  set_tests_properties(mol_seq_midi_import_validate PROPERTIES DEPENDS mol_seq_midi_import)
  add_test(NAME mol_seq_midi_export_second
           COMMAND mol-seq midi-export "${_mol_sequence_first}" "${_mol_sequence_midi_second}")
  set_tests_properties(mol_seq_midi_export_second PROPERTIES DEPENDS mol_seq_json_to_binary)
  add_test(
    NAME mol_seq_midi_determinism
    COMMAND ${CMAKE_COMMAND} -DFIRST=${_mol_sequence_midi} -DSECOND=${_mol_sequence_midi_second}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_roundtrip.cmake)
  set_tests_properties(mol_seq_midi_determinism
                       PROPERTIES DEPENDS "mol_seq_midi_export;mol_seq_midi_export_second")
  add_test(
    NAME mol_seq_midi_semantics
    COMMAND ${CMAKE_COMMAND} -DMOL_SEQ=$<TARGET_FILE:mol-seq>
            -DINPUT=${_mol_sequence_midi_import}
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/midi-semantics.json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_midi.cmake)
  set_tests_properties(mol_seq_midi_semantics PROPERTIES DEPENDS mol_seq_midi_import)
  add_test(
    NAME mol_seq_midi_corruption
    COMMAND ${CMAKE_COMMAND} -DMOL_SEQ=$<TARGET_FILE:mol-seq>
            -DINPUT=${CMAKE_CURRENT_BINARY_DIR}/corrupt.mid
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/corrupt.molseq
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_bad_midi.cmake)
  add_test(NAME mol_seq_trim
           COMMAND mol-seq trim "${_mol_sequence_first}" "${_mol_sequence_trimmed}" 12000 60000)
  set_tests_properties(mol_seq_trim PROPERTIES DEPENDS mol_seq_json_to_binary)
  add_test(NAME mol_seq_quantize
           COMMAND mol-seq quantize "${_mol_sequence_trimmed}" "${_mol_sequence_quantized}" 6000)
  set_tests_properties(mol_seq_quantize PROPERTIES DEPENDS mol_seq_trim)
  add_test(NAME mol_seq_merge
           COMMAND mol-seq merge "${_mol_sequence_merged}" "${_mol_sequence_trimmed}"
                   "${_mol_sequence_quantized}")
  set_tests_properties(mol_seq_merge PROPERTIES DEPENDS "mol_seq_trim;mol_seq_quantize")
  add_test(NAME mol_seq_merge_validate COMMAND mol-seq validate "${_mol_sequence_merged}")
  set_tests_properties(mol_seq_merge_validate PROPERTIES DEPENDS mol_seq_merge)
endif()
