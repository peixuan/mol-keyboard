# SPDX-License-Identifier: Apache-2.0

if(WIN32)
  set(_mol_physical_input_source physical_input_windows.cpp)
elseif(APPLE)
  set(_mol_physical_input_source physical_input_macos.cpp)
else()
  set(_mol_physical_input_source physical_input_linux.cpp)
endif()

add_library(mol_daemon_runtime STATIC audio_runtime.cpp ${_mol_physical_input_source})
find_package(Threads REQUIRED)
target_include_directories(mol_daemon_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_daemon_runtime PUBLIC cxx_std_17)
target_link_libraries(mol_daemon_runtime PUBLIC mol::service_control mol::miniaudio Threads::Threads)
if(WIN32)
  target_link_libraries(mol_daemon_runtime PRIVATE user32)
elseif(APPLE)
  target_link_libraries(mol_daemon_runtime PRIVATE "-framework CoreFoundation" "-framework IOKit")
endif()
set_target_properties(mol_daemon_runtime PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_daemon_runtime)
mol_enable_optional_instrumentation(mol_daemon_runtime)

add_executable(mol-keyboardd main.cpp)
target_link_libraries(mol-keyboardd PRIVATE mol_daemon_runtime mol::local_ipc)
set_target_properties(mol-keyboardd PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol-keyboardd)
mol_enable_optional_instrumentation(mol-keyboardd)
install(TARGETS mol-keyboardd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  FILES ${PROJECT_SOURCE_DIR}/packaging/systemd/mol-keyboardd.service
        ${PROJECT_SOURCE_DIR}/packaging/launchd/cn.zhangpeixuan.molkeyboard.daemon.plist
        ${PROJECT_SOURCE_DIR}/packaging/windows/install-user-startup.ps1
        ${PROJECT_SOURCE_DIR}/packaging/windows/uninstall-user-startup.ps1
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/service)

if(MOL_BUILD_TESTS)
  add_executable(mol_daemon_runtime_tests test_audio_runtime.cpp)
  target_link_libraries(mol_daemon_runtime_tests PRIVATE mol_daemon_runtime)
  set_target_properties(mol_daemon_runtime_tests PROPERTIES CXX_EXTENSIONS OFF
                                                            CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_daemon_runtime_tests)
  mol_enable_optional_instrumentation(mol_daemon_runtime_tests)
  add_test(NAME mol_daemon_runtime_tests COMMAND mol_daemon_runtime_tests)

  add_executable(mol_daemon_process_tests test_daemon_process.cpp)
  add_dependencies(mol_daemon_process_tests mol-keyboardd)
  target_link_libraries(mol_daemon_process_tests PRIVATE mol::local_ipc mol::json Threads::Threads)
  set_target_properties(mol_daemon_process_tests PROPERTIES CXX_EXTENSIONS OFF
                                                            CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_daemon_process_tests)
  mol_enable_optional_instrumentation(mol_daemon_process_tests)
  if(WIN32)
    set(_mol_daemon_test_endpoint "\\\\.\\pipe\\mol-keyboard-ctest")
  else()
    string(SHA256 _mol_daemon_test_hash "${CMAKE_CURRENT_BINARY_DIR}")
    string(SUBSTRING "${_mol_daemon_test_hash}" 0 12 _mol_daemon_test_hash)
    set(_mol_daemon_test_endpoint "/tmp/mol-keyboard-${_mol_daemon_test_hash}.sock")
  endif()
  add_test(
    NAME mol_daemon_process_tests
    COMMAND mol_daemon_process_tests $<TARGET_FILE:mol-keyboardd>
            ${CMAKE_CURRENT_BINARY_DIR}/daemon-process-state "${_mol_daemon_test_endpoint}")
  set_tests_properties(mol_daemon_process_tests PROPERTIES RUN_SERIAL TRUE TIMEOUT 20)
endif()
