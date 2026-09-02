# SPDX-License-Identifier: Apache-2.0

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Fuzz targets require Clang and libFuzzer")
endif()

if(WIN32)
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-resource-dir
    OUTPUT_VARIABLE _mol_clang_resource_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  file(GLOB _mol_asan_runtime
       "${_mol_clang_resource_dir}/lib/windows/clang_rt.asan_dynamic-*.dll")
  list(LENGTH _mol_asan_runtime _mol_asan_runtime_count)
  if(NOT _mol_asan_runtime_count EQUAL 1)
    message(FATAL_ERROR "Expected one Clang ASan runtime, found ${_mol_asan_runtime_count}")
  endif()
endif()

function(mol_add_fuzzer target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE mol::core)
  target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
  target_link_options(${target} PRIVATE -fsanitize=fuzzer)
  set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
  mol_enable_warnings(${target})
  mol_enable_optional_instrumentation(${target})
  if(WIN32)
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_mol_asan_runtime}
              $<TARGET_FILE_DIR:${target}>
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_mol_asan_runtime}
              ${CMAKE_CURRENT_BINARY_DIR}/..)
  endif()
endfunction()

mol_add_fuzzer(fuzz_patch_parser fuzz_patch_parser.c)
mol_add_fuzzer(fuzz_sequence_parser fuzz_sequence_parser.c)

set(_mol_patch_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus/patch_parser_ci")
set(_mol_patch_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/patch_parser")
file(MAKE_DIRECTORY "${_mol_patch_corpus}" "${_mol_patch_artifacts}")
configure_file(${PROJECT_SOURCE_DIR}/patches/builtin/grand-piano.molpatch.json
               ${_mol_patch_corpus}/grand-piano.molpatch.json COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/patches/compiled/grand-piano.molpatch
               ${_mol_patch_corpus}/grand-piano.molpatch COPYONLY)

add_test(
  NAME fuzz_patch_parser_smoke
  COMMAND
    fuzz_patch_parser -max_total_time=20 -max_len=16384
    -artifact_prefix=${_mol_patch_artifacts}/ ${_mol_patch_corpus})
set_tests_properties(fuzz_patch_parser_smoke PROPERTIES TIMEOUT 45)

set(_mol_sequence_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus/sequence_parser_ci")
set(_mol_sequence_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/sequence_parser")
file(MAKE_DIRECTORY "${_mol_sequence_corpus}" "${_mol_sequence_artifacts}")
configure_file(${PROJECT_SOURCE_DIR}/examples/sequences/scale-study.molseq
               ${_mol_sequence_corpus}/scale-study.molseq COPYONLY)

add_test(
  NAME fuzz_sequence_parser_smoke
  COMMAND
    fuzz_sequence_parser -max_total_time=20 -max_len=65536 -timeout=5
    -artifact_prefix=${_mol_sequence_artifacts}/ ${_mol_sequence_corpus})
set_tests_properties(fuzz_sequence_parser_smoke PROPERTIES TIMEOUT 45)
