# SPDX-License-Identifier: Apache-2.0

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Fuzz targets require Clang and libFuzzer")
endif()

add_executable(fuzz_patch_parser fuzz_patch_parser.c)
target_link_libraries(fuzz_patch_parser PRIVATE mol::core)
target_compile_options(fuzz_patch_parser PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_patch_parser PRIVATE -fsanitize=fuzzer)
set_target_properties(fuzz_patch_parser PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(fuzz_patch_parser)
mol_enable_optional_instrumentation(fuzz_patch_parser)
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
  add_custom_command(
    TARGET fuzz_patch_parser
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_mol_asan_runtime}
            $<TARGET_FILE_DIR:fuzz_patch_parser>)
endif()

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
