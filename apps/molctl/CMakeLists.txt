# SPDX-License-Identifier: Apache-2.0

add_executable(molctl main.cpp)
target_compile_features(molctl PRIVATE cxx_std_17)
target_link_libraries(molctl PRIVATE mol::local_ipc mol::json)
set_target_properties(molctl PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
mol_enable_warnings(molctl)
mol_enable_optional_instrumentation(molctl)
install(TARGETS molctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  add_test(NAME molctl_help COMMAND molctl --help)
  set_tests_properties(molctl_help PROPERTIES PASS_REGULAR_EXPRESSION "Commands:")
  add_test(NAME molctl_reject_negative_gesture COMMAND molctl note off --gesture -1)
  set_tests_properties(molctl_reject_negative_gesture PROPERTIES WILL_FAIL TRUE)
  add_test(NAME molctl_reject_unknown_option COMMAND molctl arpeggiator up --bogus 1)
  set_tests_properties(molctl_reject_unknown_option PROPERTIES WILL_FAIL TRUE)
  add_test(NAME molctl_reject_duplicate_option
           COMMAND molctl note on 60 --velocity 0.5 --velocity 0.6)
  set_tests_properties(molctl_reject_duplicate_option PROPERTIES WILL_FAIL TRUE)
  if(TARGET mol-render)
    add_dependencies(molctl mol-render)
    add_test(
      NAME molctl_render
      COMMAND molctl render ${PROJECT_SOURCE_DIR}/examples/sequences/scale-study.molseq
              --output ${CMAKE_CURRENT_BINARY_DIR}/molctl-scale.wav)
    set_tests_properties(molctl_render PROPERTIES PASS_REGULAR_EXPRESSION
                                                  "non_finite_samples=0")
  endif()
  if(APPLE)
    add_test(
      NAME mol_macos_launchd_service_smoke
      COMMAND ${PROJECT_SOURCE_DIR}/platforms/macos/run-launchd-smoke.sh
              $<TARGET_FILE:mol-keyboardd> $<TARGET_FILE:molctl>)
    set_tests_properties(mol_macos_launchd_service_smoke
                         PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
      add_test(
        NAME mol_macos_launchd_service_simulation
        COMMAND
          ${Python3_EXECUTABLE}
          ${PROJECT_SOURCE_DIR}/tests/integration/test_macos_launchd_sim.py
          --runner ${PROJECT_SOURCE_DIR}/platforms/macos/run-launchd-smoke.sh
          --daemon $<TARGET_FILE:mol-keyboardd> --controller $<TARGET_FILE:molctl>)
      set_tests_properties(mol_macos_launchd_service_simulation
                           PROPERTIES RUN_SERIAL TRUE TIMEOUT 50)
    endif()
    add_test(
      NAME mol_linux_systemd_user_service_smoke
      COMMAND ${PROJECT_SOURCE_DIR}/platforms/linux/run-systemd-user-smoke.sh
              $<TARGET_FILE:mol-keyboardd> $<TARGET_FILE:molctl>)
    set_tests_properties(
      mol_linux_systemd_user_service_smoke
      PROPERTIES RUN_SERIAL TRUE TIMEOUT 50 SKIP_RETURN_CODE 77)
  endif()
endif()
