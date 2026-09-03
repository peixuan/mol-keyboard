# SPDX-License-Identifier: Apache-2.0

add_executable(mol-latency-probe main.c ${PROJECT_SOURCE_DIR}/apps/mol-render/sha256.c)
target_include_directories(mol-latency-probe PRIVATE ${PROJECT_SOURCE_DIR}/apps/mol-render)
set_target_properties(mol-latency-probe PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
target_compile_features(mol-latency-probe PRIVATE c_std_11)
mol_enable_warnings(mol-latency-probe)
if(WIN32)
  target_compile_definitions(mol-latency-probe PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
  target_link_libraries(mol-latency-probe PRIVATE m)
endif()

install(TARGETS mol-latency-probe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  set(_mol_latency_fixture ${CMAKE_CURRENT_BINARY_DIR}/latency-fixture.wav)
  set(_mol_latency_report ${CMAKE_CURRENT_BINARY_DIR}/latency-report.json)
  set(_mol_latency_metadata
      --route wired-reference --device synthetic-loopback --buffer-config 128x3@48000
      --artifact-commit test-fixture --minimum-events 20 --max-latency-ms 100
      --refractory-ms 300)
  add_test(NAME mol_latency_probe_generate
           COMMAND mol-latency-probe --generate-fixture ${_mol_latency_fixture})
  add_test(
    NAME mol_latency_probe_analyze
    COMMAND mol-latency-probe ${_mol_latency_fixture} ${_mol_latency_metadata}
            --p95-limit-ms 30 --report ${_mol_latency_report})
  set_tests_properties(mol_latency_probe_analyze PROPERTIES DEPENDS mol_latency_probe_generate)
  add_test(
    NAME mol_latency_probe_verify
    COMMAND
      ${CMAKE_COMMAND} -DREPORT_PATH=${_mol_latency_report} -P
      ${CMAKE_CURRENT_SOURCE_DIR}/verify_report.cmake)
  set_tests_properties(mol_latency_probe_verify PROPERTIES DEPENDS mol_latency_probe_analyze)
  add_test(
    NAME mol_latency_probe_limit_reject
    COMMAND mol-latency-probe ${_mol_latency_fixture} ${_mol_latency_metadata}
            --p95-limit-ms 20 --report ${CMAKE_CURRENT_BINARY_DIR}/latency-rejected.json)
  set_tests_properties(mol_latency_probe_limit_reject
                       PROPERTIES DEPENDS mol_latency_probe_generate WILL_FAIL TRUE)
  add_test(
    NAME mol_latency_probe_corrupt_reject
    COMMAND mol-latency-probe ${PROJECT_SOURCE_DIR}/examples/sequences/scale-study.molseq
            ${_mol_latency_metadata} --report ${CMAKE_CURRENT_BINARY_DIR}/latency-corrupt.json)
  set_tests_properties(mol_latency_probe_corrupt_reject PROPERTIES WILL_FAIL TRUE)
endif()
