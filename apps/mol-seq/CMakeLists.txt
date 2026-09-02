# SPDX-License-Identifier: Apache-2.0

add_executable(mol-seq main.cpp json.cpp sequence_document.cpp)
target_link_libraries(mol-seq PRIVATE mol::core)
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
endif()
