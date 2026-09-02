# SPDX-License-Identifier: Apache-2.0

add_executable(mol-play main.cpp)
target_compile_features(mol-play PRIVATE cxx_std_17)
target_link_libraries(mol-play PRIVATE mol::core mol::miniaudio)
mol_enable_warnings(mol-play)

install(TARGETS mol-play RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  add_test(NAME mol_play_device_list COMMAND mol-play --list-devices --null-backend)
  set_tests_properties(mol_play_device_list PROPERTIES PASS_REGULAR_EXPRESSION
                                                       "backend=Null")

  add_test(
    NAME mol_play_null_realtime
    COMMAND mol-play --null-backend --duration 1.05 --note 60 --velocity 0.5)
  set_tests_properties(
    mol_play_null_realtime
    PROPERTIES PASS_REGULAR_EXPRESSION
               "frequency_hz=26[0-3]\\.[0-9]+.*render_failures=0.*non_finite=0")
endif()
