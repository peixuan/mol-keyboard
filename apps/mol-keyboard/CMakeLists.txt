# SPDX-License-Identifier: Apache-2.0

add_library(mol_desktop_web_server STATIC desktop_web_server.cpp)
target_include_directories(mol_desktop_web_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_desktop_web_server PUBLIC cxx_std_17)
target_link_libraries(mol_desktop_web_server PUBLIC ws2_32)
set_target_properties(mol_desktop_web_server PROPERTIES CXX_EXTENSIONS OFF
                                                        CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_desktop_web_server)
mol_enable_optional_instrumentation(mol_desktop_web_server)

if(MOL_BUILD_TESTS)
  add_executable(mol_desktop_web_server_tests test_desktop_web_server.cpp)
  target_link_libraries(mol_desktop_web_server_tests PRIVATE mol_desktop_web_server)
  set_target_properties(mol_desktop_web_server_tests PROPERTIES CXX_EXTENSIONS OFF
                                                                CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_desktop_web_server_tests)
  mol_enable_optional_instrumentation(mol_desktop_web_server_tests)
  add_test(NAME mol_desktop_web_server_tests COMMAND mol_desktop_web_server_tests)
  set_tests_properties(mol_desktop_web_server_tests PROPERTIES TIMEOUT 10)
endif()
