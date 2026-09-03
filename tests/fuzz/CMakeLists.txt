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
mol_add_fuzzer(fuzz_wire_parser fuzz_wire_parser.c)
mol_add_fuzzer(fuzz_jsonrpc_dispatch fuzz_jsonrpc_dispatch.cpp)
mol_add_fuzzer(fuzz_config_parser fuzz_config_parser.cpp)
mol_add_fuzzer(fuzz_midi_parser fuzz_midi_parser.cpp)
mol_add_fuzzer(fuzz_latency_capture fuzz_latency_capture.c)
target_sources(fuzz_latency_capture PRIVATE ${PROJECT_SOURCE_DIR}/apps/mol-render/sha256.c)
target_include_directories(fuzz_latency_capture PRIVATE ${PROJECT_SOURCE_DIR}/apps/mol-render)
if(WIN32)
  target_compile_definitions(fuzz_latency_capture PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(fuzz_jsonrpc_dispatch PRIVATE mol::jsonrpc)
target_link_libraries(fuzz_config_parser PRIVATE mol::service_control)
target_link_libraries(fuzz_midi_parser PRIVATE mol::midi)
target_compile_features(fuzz_jsonrpc_dispatch PRIVATE cxx_std_17)
target_compile_features(fuzz_config_parser PRIVATE cxx_std_17)
target_compile_features(fuzz_midi_parser PRIVATE cxx_std_17)
set_target_properties(
  fuzz_jsonrpc_dispatch fuzz_config_parser fuzz_midi_parser
  PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
if(WIN32)
  target_compile_definitions(
    fuzz_jsonrpc_dispatch
    PRIVATE _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION _ITERATOR_DEBUG_LEVEL=0)
  foreach(_mol_cpp_fuzzer fuzz_config_parser fuzz_midi_parser)
    target_compile_definitions(
      ${_mol_cpp_fuzzer}
      PRIVATE _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION _ITERATOR_DEBUG_LEVEL=0)
  endforeach()
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

set(_mol_wire_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/wire_parser")
file(MAKE_DIRECTORY "${_mol_wire_artifacts}")
add_test(
  NAME fuzz_wire_parser_smoke
  COMMAND fuzz_wire_parser -max_total_time=20 -max_len=64 -timeout=5
          -artifact_prefix=${_mol_wire_artifacts}/)
set_tests_properties(fuzz_wire_parser_smoke PROPERTIES TIMEOUT 45)

set(_mol_jsonrpc_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus/jsonrpc_dispatch_ci")
set(_mol_jsonrpc_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/jsonrpc_dispatch")
file(MAKE_DIRECTORY "${_mol_jsonrpc_corpus}" "${_mol_jsonrpc_artifacts}")
configure_file(seeds/jsonrpc_request.json ${_mol_jsonrpc_corpus}/request.json COPYONLY)
add_test(
  NAME fuzz_jsonrpc_dispatch_smoke
  COMMAND
    fuzz_jsonrpc_dispatch -max_total_time=20 -max_len=65536 -timeout=5
    -artifact_prefix=${_mol_jsonrpc_artifacts}/ ${_mol_jsonrpc_corpus})
set_tests_properties(fuzz_jsonrpc_dispatch_smoke PROPERTIES TIMEOUT 45)

set(_mol_config_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus/config_parser_ci")
set(_mol_config_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/config_parser")
file(MAKE_DIRECTORY "${_mol_config_corpus}" "${_mol_config_artifacts}")
configure_file(seeds/config.json ${_mol_config_corpus}/config.json COPYONLY)
add_test(
  NAME fuzz_config_parser_smoke
  COMMAND
    fuzz_config_parser -max_total_time=20 -max_len=65536 -timeout=5
    -artifact_prefix=${_mol_config_artifacts}/ ${_mol_config_corpus})
set_tests_properties(fuzz_config_parser_smoke PROPERTIES TIMEOUT 45)

set(_mol_midi_corpus "${CMAKE_CURRENT_BINARY_DIR}/corpus/midi_parser_ci")
set(_mol_midi_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/midi_parser")
file(MAKE_DIRECTORY "${_mol_midi_corpus}" "${_mol_midi_artifacts}")
configure_file(seeds/midi_header ${_mol_midi_corpus}/midi_header COPYONLY)
add_test(
  NAME fuzz_midi_parser_smoke
  COMMAND
    fuzz_midi_parser -max_total_time=20 -max_len=1048576 -timeout=5
    -artifact_prefix=${_mol_midi_artifacts}/ ${_mol_midi_corpus})
set_tests_properties(fuzz_midi_parser_smoke PROPERTIES TIMEOUT 45)

set(_mol_latency_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifacts/latency_capture")
file(MAKE_DIRECTORY "${_mol_latency_artifacts}")
add_test(
  NAME fuzz_latency_capture_smoke
  COMMAND fuzz_latency_capture -max_total_time=20 -max_len=1048576 -timeout=5
          -artifact_prefix=${_mol_latency_artifacts}/)
set_tests_properties(fuzz_latency_capture_smoke PROPERTIES TIMEOUT 45)
