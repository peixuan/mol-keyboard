# SPDX-License-Identifier: Apache-2.0

add_library(mol_desktop_web_server STATIC desktop_web_server.cpp)
target_include_directories(mol_desktop_web_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_desktop_web_server PUBLIC cxx_std_17)
target_link_libraries(mol_desktop_web_server PUBLIC ws2_32)
set_target_properties(mol_desktop_web_server PROPERTIES CXX_EXTENSIONS OFF
                                                        CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_desktop_web_server)
mol_enable_optional_instrumentation(mol_desktop_web_server)

add_executable(mol-keyboard WIN32 main_windows.cpp)
target_link_libraries(mol-keyboard PRIVATE mol_desktop_web_server shell32 user32)
target_compile_definitions(
  mol-keyboard PRIVATE MOL_DEVELOPMENT_WEB_ROOT="${PROJECT_SOURCE_DIR}/apps/web/dist")
set_target_properties(mol-keyboard PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol-keyboard)
mol_enable_optional_instrumentation(mol-keyboard)
install(TARGETS mol-keyboard RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  add_executable(mol_desktop_web_server_tests test_desktop_web_server.cpp)
  target_link_libraries(mol_desktop_web_server_tests PRIVATE mol_desktop_web_server)
  set_target_properties(mol_desktop_web_server_tests PROPERTIES CXX_EXTENSIONS OFF
                                                                CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_desktop_web_server_tests)
  mol_enable_optional_instrumentation(mol_desktop_web_server_tests)
  add_test(NAME mol_desktop_web_server_tests COMMAND mol_desktop_web_server_tests)
  set_tests_properties(mol_desktop_web_server_tests PROPERTIES TIMEOUT 10)

  add_executable(mol_desktop_fake_browser test_fake_browser.cpp)
  target_compile_features(mol_desktop_fake_browser PRIVATE cxx_std_17)
  target_link_libraries(mol_desktop_fake_browser PRIVATE ws2_32)
  set_target_properties(mol_desktop_fake_browser PROPERTIES CXX_EXTENSIONS OFF
                                                           CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_desktop_fake_browser)
  mol_enable_optional_instrumentation(mol_desktop_fake_browser)
  add_test(
    NAME mol_desktop_launcher_tests
    COMMAND mol-keyboard --web-root ${PROJECT_SOURCE_DIR}/apps/web/dist --browser
            $<TARGET_FILE:mol_desktop_fake_browser>)
  set_tests_properties(mol_desktop_launcher_tests PROPERTIES TIMEOUT 10)
endif()
