# SPDX-License-Identifier: Apache-2.0

add_executable(mol-audio-analyze main.c)
set_target_properties(mol-audio-analyze PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
target_compile_features(mol-audio-analyze PRIVATE c_std_11)
mol_enable_warnings(mol-audio-analyze)
if(WIN32)
  target_compile_definitions(mol-audio-analyze PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
  target_link_libraries(mol-audio-analyze PRIVATE m)
endif()

install(TARGETS mol-audio-analyze RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  set(_mol_audio_analyze_arguments
      ${PROJECT_BINARY_DIR}/apps/mol-render/test-c4.wav --expect-frequency 261.6256
      --frequency-tolerance 2.0 --max-dc 0.01 --max-peak 0.8913 --max-step 0.25)
  if(MOL_ENABLE_CHORUS OR MOL_ENABLE_REVERB)
    list(APPEND _mol_audio_analyze_arguments --require-stereo)
  endif()
  add_test(
    NAME mol_audio_analyze
    COMMAND mol-audio-analyze ${_mol_audio_analyze_arguments})
  set_tests_properties(mol_audio_analyze PROPERTIES DEPENDS mol_render_wav)
endif()
