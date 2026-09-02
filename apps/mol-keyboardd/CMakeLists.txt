# SPDX-License-Identifier: Apache-2.0

add_library(mol_daemon_runtime STATIC audio_runtime.cpp)
target_include_directories(mol_daemon_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_daemon_runtime PUBLIC cxx_std_17)
target_link_libraries(mol_daemon_runtime PUBLIC mol::service_control mol::miniaudio)
set_target_properties(mol_daemon_runtime PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_daemon_runtime)
mol_enable_optional_instrumentation(mol_daemon_runtime)

if(MOL_BUILD_TESTS)
  add_executable(mol_daemon_runtime_tests test_audio_runtime.cpp)
  target_link_libraries(mol_daemon_runtime_tests PRIVATE mol_daemon_runtime)
  set_target_properties(mol_daemon_runtime_tests PROPERTIES CXX_EXTENSIONS OFF
                                                            CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_daemon_runtime_tests)
  mol_enable_optional_instrumentation(mol_daemon_runtime_tests)
  add_test(NAME mol_daemon_runtime_tests COMMAND mol_daemon_runtime_tests)
endif()
