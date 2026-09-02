# SPDX-License-Identifier: Apache-2.0

add_executable(mol-render main.c)
target_link_libraries(mol-render PRIVATE mol::core)
set_target_properties(mol-render PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol-render)
if(MSVC)
  target_compile_definitions(mol-render PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
  target_link_libraries(mol-render PRIVATE m)
endif()

install(TARGETS mol-render RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  set(_mol_test_wav "${CMAKE_CURRENT_BINARY_DIR}/test-c4.wav")
  add_test(
    NAME mol_render_wav
    COMMAND mol-render --output "${_mol_test_wav}" --duration 1 --note 60 --channels 2)
  add_test(
    NAME mol_render_wav_verify
    COMMAND ${CMAKE_COMMAND} -DWAV_PATH=${_mol_test_wav}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_wav.cmake)
  set_tests_properties(mol_render_wav_verify PROPERTIES DEPENDS mol_render_wav)
endif()
