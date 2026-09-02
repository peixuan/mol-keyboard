# SPDX-License-Identifier: Apache-2.0

add_executable(mol_core_tests unit/test_engine.c)
target_link_libraries(mol_core_tests PRIVATE mol::core)
set_target_properties(mol_core_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_core_tests)
mol_enable_optional_instrumentation(mol_core_tests)
add_test(NAME mol_core_tests COMMAND mol_core_tests)

add_executable(mol_audio_tests unit/test_audio.c)
target_link_libraries(mol_audio_tests PRIVATE mol::core)
set_target_properties(mol_audio_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_audio_tests)
mol_enable_optional_instrumentation(mol_audio_tests)
add_test(NAME mol_audio_tests COMMAND mol_audio_tests)

add_executable(mol_dsp_tests unit/test_dsp.c)
target_link_libraries(mol_dsp_tests PRIVATE mol::core)
target_include_directories(mol_dsp_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/dsp)
set_target_properties(mol_dsp_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_dsp_tests)
mol_enable_optional_instrumentation(mol_dsp_tests)
add_test(NAME mol_dsp_tests COMMAND mol_dsp_tests)

add_executable(mol_music_tests unit/test_music.c)
target_link_libraries(mol_music_tests PRIVATE mol::core)
set_target_properties(mol_music_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_music_tests)
mol_enable_optional_instrumentation(mol_music_tests)
add_test(NAME mol_music_tests COMMAND mol_music_tests)

add_executable(mol_semantics_tests unit/test_semantics.c)
target_link_libraries(mol_semantics_tests PRIVATE mol::core)
set_target_properties(mol_semantics_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_semantics_tests)
mol_enable_optional_instrumentation(mol_semantics_tests)
add_test(NAME mol_semantics_tests COMMAND mol_semantics_tests)

add_executable(mol_transport_tests unit/test_transport.c)
target_link_libraries(mol_transport_tests PRIVATE mol::core)
set_target_properties(mol_transport_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_transport_tests)
mol_enable_optional_instrumentation(mol_transport_tests)
add_test(NAME mol_transport_tests COMMAND mol_transport_tests)

add_executable(mol_arpeggiator_tests unit/test_arpeggiator.c)
target_link_libraries(mol_arpeggiator_tests PRIVATE mol::core)
set_target_properties(mol_arpeggiator_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_arpeggiator_tests)
mol_enable_optional_instrumentation(mol_arpeggiator_tests)
add_test(NAME mol_arpeggiator_tests COMMAND mol_arpeggiator_tests)

add_executable(mol_portamento_tests unit/test_portamento.c)
target_link_libraries(mol_portamento_tests PRIVATE mol::core)
set_target_properties(mol_portamento_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_portamento_tests)
mol_enable_optional_instrumentation(mol_portamento_tests)
add_test(NAME mol_portamento_tests COMMAND mol_portamento_tests)

add_executable(mol_event_trace conformance/event_trace.c)
target_link_libraries(mol_event_trace PRIVATE mol::core)
set_target_properties(mol_event_trace PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_event_trace)
mol_enable_optional_instrumentation(mol_event_trace)
add_test(
  NAME mol_event_trace_conformance
  COMMAND
    ${CMAKE_COMMAND} -DMOL_EXECUTABLE=$<TARGET_FILE:mol_event_trace>
    -DMOL_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}
    -DMOL_EXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/golden/m2_event_trace.txt -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_event_trace.cmake)

add_executable(mol_property_tests property/test_properties.c)
target_link_libraries(mol_property_tests PRIVATE mol::core)
set_target_properties(mol_property_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_property_tests)
mol_enable_optional_instrumentation(mol_property_tests)
add_test(NAME mol_property_tests COMMAND mol_property_tests)

add_executable(mol_platform_audio_runtime_tests unit/test_platform_audio_runtime.c)
target_link_libraries(mol_platform_audio_runtime_tests PRIVATE mol::platform_audio_runtime)
set_target_properties(mol_platform_audio_runtime_tests PROPERTIES C_EXTENSIONS OFF
                                                          C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_platform_audio_runtime_tests)
mol_enable_optional_instrumentation(mol_platform_audio_runtime_tests)
add_test(NAME mol_platform_audio_runtime_tests COMMAND mol_platform_audio_runtime_tests)

add_executable(mol_c_consumer consumers/c_consumer.c)
target_link_libraries(mol_c_consumer PRIVATE mol::core)
set_target_properties(mol_c_consumer PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_c_consumer)
add_test(NAME mol_c_consumer COMMAND mol_c_consumer)

add_executable(mol_cpp_consumer consumers/cpp_consumer.cpp)
target_link_libraries(mol_cpp_consumer PRIVATE mol::core)
target_compile_features(mol_cpp_consumer PRIVATE cxx_std_17)
set_target_properties(mol_cpp_consumer PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_cpp_consumer)
add_test(NAME mol_cpp_consumer COMMAND mol_cpp_consumer)
