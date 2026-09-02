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
endif()
