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

add_executable(mol_effect_tests unit/test_effects.c)
target_link_libraries(mol_effect_tests PRIVATE mol::core)
target_include_directories(mol_effect_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/dsp
                                                    ${PROJECT_SOURCE_DIR}/src/effects)
set_target_properties(mol_effect_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_effect_tests)
mol_enable_optional_instrumentation(mol_effect_tests)
add_test(NAME mol_effect_tests COMMAND mol_effect_tests)

add_executable(mol_patch_tests unit/test_patch.c)
target_link_libraries(mol_patch_tests PRIVATE mol::core)
set_target_properties(mol_patch_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_patch_tests)
mol_enable_optional_instrumentation(mol_patch_tests)
add_test(NAME mol_patch_tests COMMAND mol_patch_tests)
if(MOL_BUILD_TOOLS AND NOT CMAKE_CROSSCOMPILING)
  add_test(
    NAME mol_patchc_builtin_compile
    COMMAND
      ${CMAKE_COMMAND} -DMOL_PATCHC=$<TARGET_FILE:mol-patchc>
      -DMOL_PATCH_DIR=${PROJECT_SOURCE_DIR}/patches/builtin
      -DMOL_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/compiled-patches -P
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_patch_compiler.cmake)
endif()

add_executable(mol_sequence_tests unit/test_sequence.c)
target_link_libraries(mol_sequence_tests PRIVATE mol::core)
set_target_properties(mol_sequence_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_sequence_tests)
mol_enable_optional_instrumentation(mol_sequence_tests)
add_test(NAME mol_sequence_tests COMMAND mol_sequence_tests)

add_executable(mol_wire_tests unit/test_wire.c)
target_link_libraries(mol_wire_tests PRIVATE mol::core)
set_target_properties(mol_wire_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_wire_tests)
mol_enable_optional_instrumentation(mol_wire_tests)
add_test(NAME mol_wire_tests COMMAND mol_wire_tests)

add_executable(
  mol_esp32_matrix_tests unit/test_esp32_matrix.c
                         ${PROJECT_SOURCE_DIR}/platforms/esp32/main/matrix_logic.c)
target_include_directories(mol_esp32_matrix_tests
                           PRIVATE ${PROJECT_SOURCE_DIR}/platforms/esp32/main)
set_target_properties(mol_esp32_matrix_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_esp32_matrix_tests)
mol_enable_optional_instrumentation(mol_esp32_matrix_tests)
add_test(NAME mol_esp32_matrix_tests COMMAND mol_esp32_matrix_tests)

add_executable(
  mol_esp32_settings_tests unit/test_esp32_settings.c
                           ${PROJECT_SOURCE_DIR}/platforms/esp32/main/device_settings.c)
target_include_directories(mol_esp32_settings_tests
                           PRIVATE ${PROJECT_SOURCE_DIR}/platforms/esp32/main)
target_link_libraries(mol_esp32_settings_tests PRIVATE mol::core)
set_target_properties(mol_esp32_settings_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_esp32_settings_tests)
mol_enable_optional_instrumentation(mol_esp32_settings_tests)
add_test(NAME mol_esp32_settings_tests COMMAND mol_esp32_settings_tests)

add_executable(
  mol_esp32_web_protocol_tests unit/test_esp32_web_protocol.c
                               ${PROJECT_SOURCE_DIR}/platforms/esp32/main/device_settings.c
                               ${PROJECT_SOURCE_DIR}/platforms/esp32/main/web_config_protocol.c)
target_include_directories(mol_esp32_web_protocol_tests
                           PRIVATE ${PROJECT_SOURCE_DIR}/platforms/esp32/main)
target_link_libraries(mol_esp32_web_protocol_tests PRIVATE mol::core)
set_target_properties(mol_esp32_web_protocol_tests PROPERTIES C_EXTENSIONS OFF
                                                          C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_esp32_web_protocol_tests)
mol_enable_optional_instrumentation(mol_esp32_web_protocol_tests)
add_test(NAME mol_esp32_web_protocol_tests COMMAND mol_esp32_web_protocol_tests)

add_executable(
  mol_esp32_sequence_store_tests unit/test_esp32_sequence_store.c
                                 ${PROJECT_SOURCE_DIR}/platforms/esp32/main/sequence_store.c)
target_include_directories(mol_esp32_sequence_store_tests
                           PRIVATE ${PROJECT_SOURCE_DIR}/platforms/esp32/main)
target_link_libraries(mol_esp32_sequence_store_tests PRIVATE mol::core)
set_target_properties(mol_esp32_sequence_store_tests PROPERTIES C_EXTENSIONS OFF
                                                         C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_esp32_sequence_store_tests)
mol_enable_optional_instrumentation(mol_esp32_sequence_store_tests)
if(MSVC)
  target_compile_definitions(mol_esp32_sequence_store_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME mol_esp32_sequence_store_tests COMMAND mol_esp32_sequence_store_tests)

add_executable(
  mol_esp32_hid_keyboard_tests unit/test_esp32_hid_keyboard.c
                               ${PROJECT_SOURCE_DIR}/platforms/esp32/main/hid_keyboard.c)
target_include_directories(mol_esp32_hid_keyboard_tests
                           PRIVATE ${PROJECT_SOURCE_DIR}/platforms/esp32/main)
target_link_libraries(mol_esp32_hid_keyboard_tests PRIVATE mol::core)
set_target_properties(mol_esp32_hid_keyboard_tests PROPERTIES C_EXTENSIONS OFF
                                                         C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_esp32_hid_keyboard_tests)
mol_enable_optional_instrumentation(mol_esp32_hid_keyboard_tests)
add_test(NAME mol_esp32_hid_keyboard_tests COMMAND mol_esp32_hid_keyboard_tests)

add_test(
  NAME mol_harmony_project_audit
  COMMAND
    ${CMAKE_COMMAND} -DMOL_SOURCE_DIR=${PROJECT_SOURCE_DIR} -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_harmony_project.cmake)

if(TARGET mol_jsonrpc)
  add_executable(mol_jsonrpc_tests unit/test_jsonrpc.cpp)
  target_link_libraries(mol_jsonrpc_tests PRIVATE mol::jsonrpc)
  set_target_properties(mol_jsonrpc_tests PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_jsonrpc_tests)
  mol_enable_optional_instrumentation(mol_jsonrpc_tests)
  add_test(NAME mol_jsonrpc_tests COMMAND mol_jsonrpc_tests)

  add_executable(mol_service_rpc_tests unit/test_service_rpc.cpp)
  target_link_libraries(mol_service_rpc_tests PRIVATE mol::jsonrpc)
  set_target_properties(mol_service_rpc_tests PROPERTIES CXX_EXTENSIONS OFF
                                                         CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_service_rpc_tests)
  mol_enable_optional_instrumentation(mol_service_rpc_tests)
  add_test(NAME mol_service_rpc_tests COMMAND mol_service_rpc_tests)

  if(TARGET mol_service_control)
    add_executable(mol_service_backend_tests unit/test_service_backend.cpp)
    target_link_libraries(mol_service_backend_tests PRIVATE mol::service_control)
    set_target_properties(mol_service_backend_tests PROPERTIES CXX_EXTENSIONS OFF
                                                               CXX_STANDARD_REQUIRED ON)
    mol_enable_warnings(mol_service_backend_tests)
    mol_enable_optional_instrumentation(mol_service_backend_tests)
    add_test(NAME mol_service_backend_tests COMMAND mol_service_backend_tests)
  endif()
endif()

if(TARGET mol_local_ipc)
  find_package(Threads REQUIRED)
  add_executable(mol_local_ipc_tests unit/test_local_ipc.cpp)
  target_link_libraries(mol_local_ipc_tests PRIVATE mol::local_ipc Threads::Threads)
  set_target_properties(mol_local_ipc_tests PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_local_ipc_tests)
  mol_enable_optional_instrumentation(mol_local_ipc_tests)
  add_test(NAME mol_local_ipc_tests COMMAND mol_local_ipc_tests)

  add_executable(mol_loopback_websocket_tests unit/test_loopback_websocket.cpp)
  target_link_libraries(mol_loopback_websocket_tests PRIVATE mol::local_ipc Threads::Threads)
  set_target_properties(
    mol_loopback_websocket_tests PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_loopback_websocket_tests)
  mol_enable_optional_instrumentation(mol_loopback_websocket_tests)
  if(WIN32)
    target_link_libraries(mol_loopback_websocket_tests PRIVATE ws2_32)
  endif()
  add_test(NAME mol_loopback_websocket_tests COMMAND mol_loopback_websocket_tests)
endif()

add_executable(mol_recording_tests unit/test_recording.c)
target_link_libraries(mol_recording_tests PRIVATE mol::core)
set_target_properties(mol_recording_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_recording_tests)
mol_enable_optional_instrumentation(mol_recording_tests)
add_test(NAME mol_recording_tests COMMAND mol_recording_tests)

file(READ "${PROJECT_SOURCE_DIR}/examples/sequences/scale-study.molseq"
     MOL_SEQUENCE_FIXTURE_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," MOL_SEQUENCE_FIXTURE_BYTES
                     "${MOL_SEQUENCE_FIXTURE_HEX}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
  fixtures/sequence_fixture_bytes.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mol_sequence_fixture_bytes.h" @ONLY)
add_executable(mol_sequence_fixture conformance/sequence_fixture.c)
target_link_libraries(mol_sequence_fixture PRIVATE mol::core)
target_include_directories(mol_sequence_fixture PRIVATE conformance
                                                        "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(mol_sequence_fixture PRIVATE MOL_SEQUENCE_FIXTURE_STANDALONE=1)
set_target_properties(mol_sequence_fixture PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_sequence_fixture)
mol_enable_optional_instrumentation(mol_sequence_fixture)
add_test(
  NAME mol_sequence_fixture_conformance
  COMMAND
    ${CMAKE_COMMAND} -DMOL_EXECUTABLE=$<TARGET_FILE:mol_sequence_fixture>
    -DMOL_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}
    -DMOL_EXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/golden/m4_sequence_fixture.txt -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_event_trace.cmake)

add_executable(mol_instrument_tests integration/test_instruments.c)
target_link_libraries(mol_instrument_tests PRIVATE mol::core)
set_target_properties(mol_instrument_tests PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_instrument_tests)
mol_enable_optional_instrumentation(mol_instrument_tests)
add_test(NAME mol_instrument_tests COMMAND mol_instrument_tests)

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

add_executable(mol_audio_metrics conformance/audio_metrics.c)
target_link_libraries(mol_audio_metrics PRIVATE mol::core)
set_target_properties(mol_audio_metrics PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_audio_metrics)
mol_enable_optional_instrumentation(mol_audio_metrics)
if(MOL_PROFILE_STANDARD AND MOL_ENABLE_CHORUS AND MOL_ENABLE_DELAY AND MOL_ENABLE_REVERB)
  add_test(
    NAME mol_audio_metrics_conformance
    COMMAND
      ${CMAKE_COMMAND} -DMOL_EXECUTABLE=$<TARGET_FILE:mol_audio_metrics>
      -DMOL_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}
      -DMOL_EXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/golden/m3_audio_metrics.txt -P
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_audio_metrics.cmake)
endif()

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
mol_enable_optional_instrumentation(mol_c_consumer)
add_test(NAME mol_c_consumer COMMAND mol_c_consumer)

add_executable(mol_cpp_consumer consumers/cpp_consumer.cpp)
target_link_libraries(mol_cpp_consumer PRIVATE mol::core)
target_compile_features(mol_cpp_consumer PRIVATE cxx_std_17)
set_target_properties(mol_cpp_consumer PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_cpp_consumer)
mol_enable_optional_instrumentation(mol_cpp_consumer)
add_test(NAME mol_cpp_consumer COMMAND mol_cpp_consumer)

if(MOL_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()
