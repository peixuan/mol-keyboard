# SPDX-License-Identifier: Apache-2.0

add_library(mol_desktop_web_server STATIC desktop_web_server.cpp)
target_include_directories(mol_desktop_web_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_desktop_web_server PUBLIC cxx_std_17)
if(WIN32)
  target_link_libraries(mol_desktop_web_server PUBLIC ws2_32)
endif()
set_target_properties(mol_desktop_web_server PROPERTIES CXX_EXTENSIONS OFF
                                                        CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_desktop_web_server)
mol_enable_optional_instrumentation(mol_desktop_web_server)

add_library(mol_desktop_app_support STATIC desktop_app_support.cpp)
target_include_directories(mol_desktop_app_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_desktop_app_support PUBLIC cxx_std_17)
set_target_properties(mol_desktop_app_support PROPERTIES CXX_EXTENSIONS OFF
                                                        CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(mol_desktop_app_support)
mol_enable_optional_instrumentation(mol_desktop_app_support)

if(MOL_BUILD_DESKTOP_GUI)
  add_executable(mol-keyboard WIN32 MACOSX_BUNDLE main_webview.cpp)
  target_link_libraries(mol-keyboard PRIVATE mol_desktop_app_support
                                             mol_desktop_web_server wx::core wx::webview)
  target_compile_definitions(
    mol-keyboard PRIVATE MOL_DEVELOPMENT_WEB_ROOT="${PROJECT_SOURCE_DIR}/apps/web/dist")
  set_target_properties(
    mol-keyboard
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_STANDARD_REQUIRED ON
               MACOSX_BUNDLE_BUNDLE_NAME "MoL Keyboard"
               MACOSX_BUNDLE_GUI_IDENTIFIER "${MOL_APPLE_BUNDLE_IDENTIFIER}"
               MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  mol_enable_warnings(mol-keyboard)
  mol_enable_optional_instrumentation(mol-keyboard)
  install(TARGETS mol-keyboard RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                               BUNDLE DESTINATION .)

  if(APPLE)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/apps/web/dist/
            DESTINATION mol-keyboard.app/Contents/Resources/web)
  endif()

  if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
      set(_mol_webview2_arch arm64)
    else()
      set(_mol_webview2_arch x64)
    endif()
    set(_mol_webview2_loader
        "${WEBVIEW2_PACKAGE_DIR}/build/native/${_mol_webview2_arch}/WebView2Loader.dll")
    if(NOT EXISTS "${_mol_webview2_loader}")
      message(FATAL_ERROR "The checksum-locked WebView2 loader is missing")
    endif()
    add_custom_command(
      TARGET mol-keyboard POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_mol_webview2_loader}"
              "$<TARGET_FILE_DIR:mol-keyboard>/WebView2Loader.dll")
    install(FILES "${_mol_webview2_loader}" TYPE BIN)
  endif()
endif()

if(MOL_BUILD_NATIVE_DEBUG_GUI)
  add_library(mol_desktop_rpc_client STATIC desktop_rpc_client.cpp)
  target_include_directories(mol_desktop_rpc_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(mol_desktop_rpc_client PUBLIC mol::local_ipc mol::json)
  target_compile_features(mol_desktop_rpc_client PUBLIC cxx_std_17)
  set_target_properties(mol_desktop_rpc_client PROPERTIES CXX_EXTENSIONS OFF
                                                          CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_desktop_rpc_client)
  mol_enable_optional_instrumentation(mol_desktop_rpc_client)

  add_executable(mol-keyboard-debug WIN32 MACOSX_BUNDLE main_native_debug.cpp)
  target_link_libraries(mol-keyboard-debug PRIVATE mol_desktop_rpc_client wx::core)
  set_target_properties(
    mol-keyboard-debug
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_STANDARD_REQUIRED ON
               MACOSX_BUNDLE_BUNDLE_NAME "MoL Keyboard Debugger"
               MACOSX_BUNDLE_GUI_IDENTIFIER "${MOL_APPLE_BUNDLE_IDENTIFIER}.debugger"
               MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  mol_enable_warnings(mol-keyboard-debug)
  mol_enable_optional_instrumentation(mol-keyboard-debug)
  install(TARGETS mol-keyboard-debug RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                                     BUNDLE DESTINATION .)
endif()

if(MOL_BUILD_TESTS)
  add_executable(mol_desktop_web_server_tests test_desktop_web_server.cpp)
  target_link_libraries(mol_desktop_web_server_tests PRIVATE mol_desktop_web_server)
  set_target_properties(mol_desktop_web_server_tests PROPERTIES CXX_EXTENSIONS OFF
                                                                CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_desktop_web_server_tests)
  mol_enable_optional_instrumentation(mol_desktop_web_server_tests)
  add_test(NAME mol_desktop_web_server_tests COMMAND mol_desktop_web_server_tests)
  set_tests_properties(mol_desktop_web_server_tests PROPERTIES TIMEOUT 10)

  add_executable(mol_desktop_app_support_tests test_desktop_app_support.cpp)
  target_link_libraries(mol_desktop_app_support_tests PRIVATE mol_desktop_app_support)
  set_target_properties(mol_desktop_app_support_tests PROPERTIES CXX_EXTENSIONS OFF
                                                                CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_desktop_app_support_tests)
  mol_enable_optional_instrumentation(mol_desktop_app_support_tests)
  add_test(NAME mol_desktop_app_support_tests COMMAND mol_desktop_app_support_tests)
  set_tests_properties(mol_desktop_app_support_tests PROPERTIES TIMEOUT 10)

  if(MOL_BUILD_DESKTOP_GUI)
    set(_mol_webview_report "${CMAKE_CURRENT_BINARY_DIR}/webview-acceptance.txt")
    set(_mol_webview_command
        ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/integration/test_gui_acceptance.py
        --executable $<TARGET_FILE:mol-keyboard> --report ${_mol_webview_report} -- --web-root
        ${PROJECT_SOURCE_DIR}/apps/web/dist --acceptance-output ${_mol_webview_report})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_program(MOL_XVFB_RUN xvfb-run)
      if(MOL_XVFB_RUN)
        add_test(NAME mol_desktop_webview_acceptance
                 COMMAND ${MOL_XVFB_RUN} -a ${_mol_webview_command})
      elseif(DEFINED ENV{DISPLAY} AND NOT "$ENV{DISPLAY}" STREQUAL "")
        add_test(NAME mol_desktop_webview_acceptance COMMAND ${_mol_webview_command})
      else()
        message(FATAL_ERROR
                "MOL_BUILD_DESKTOP_GUI tests require DISPLAY or xvfb-run on Linux")
      endif()
    else()
      add_test(NAME mol_desktop_webview_acceptance COMMAND ${_mol_webview_command})
    endif()
    set_tests_properties(mol_desktop_webview_acceptance PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)
  endif()

  if(MOL_BUILD_NATIVE_DEBUG_GUI AND MOL_BUILD_DAEMON)
    set(_mol_native_debug_state "${CMAKE_CURRENT_BINARY_DIR}/native-debug-state")
    set(_mol_native_debug_report "${CMAKE_CURRENT_BINARY_DIR}/native-debug-acceptance.txt")
    if(WIN32)
      set(_mol_native_debug_endpoint "\\\\.\\pipe\\mol-keyboard-native-debug-ctest")
    else()
      string(SHA256 _mol_native_debug_hash "${CMAKE_CURRENT_BINARY_DIR}")
      string(SUBSTRING "${_mol_native_debug_hash}" 0 12 _mol_native_debug_hash)
      set(_mol_native_debug_endpoint "/tmp/mol-native-debug-${_mol_native_debug_hash}.sock")
    endif()
    set(_mol_native_debug_command
        ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/integration/test_native_debug_gui.py
        --daemon $<TARGET_FILE:mol-keyboardd> --gui $<TARGET_FILE:mol-keyboard-debug>
        --state-dir ${_mol_native_debug_state} --endpoint ${_mol_native_debug_endpoint}
        --report ${_mol_native_debug_report})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_program(MOL_XVFB_RUN xvfb-run)
      if(MOL_XVFB_RUN)
        add_test(NAME mol_native_debug_gui_acceptance
                 COMMAND ${MOL_XVFB_RUN} -a ${_mol_native_debug_command})
      elseif(DEFINED ENV{DISPLAY} AND NOT "$ENV{DISPLAY}" STREQUAL "")
        add_test(NAME mol_native_debug_gui_acceptance COMMAND ${_mol_native_debug_command})
      else()
        message(FATAL_ERROR
                "MOL_BUILD_NATIVE_DEBUG_GUI tests require DISPLAY or xvfb-run on Linux")
      endif()
    else()
      add_test(NAME mol_native_debug_gui_acceptance COMMAND ${_mol_native_debug_command})
    endif()
    set_tests_properties(mol_native_debug_gui_acceptance PROPERTIES RUN_SERIAL TRUE TIMEOUT 25)
  endif()
endif()
