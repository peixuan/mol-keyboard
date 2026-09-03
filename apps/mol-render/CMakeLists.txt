# SPDX-License-Identifier: Apache-2.0

add_executable(mol-render main.c sha256.c)
target_link_libraries(mol-render PRIVATE mol::core)
set_target_properties(mol-render PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
mol_enable_warnings(mol-render)
if(WIN32)
  target_compile_definitions(mol-render PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
  target_link_libraries(mol-render PRIVATE m)
endif()

install(TARGETS mol-render RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(MOL_BUILD_TESTS)
  set(_mol_test_wav "${CMAKE_CURRENT_BINARY_DIR}/test-c4.wav")
  set(_mol_test_report "${CMAKE_CURRENT_BINARY_DIR}/test-c4.wav.json")
  add_test(
    NAME mol_render_wav
    COMMAND
      mol-render --output "${_mol_test_wav}" --report "${_mol_test_report}" --duration 1
      --gate 0.25 --note 60 --channels 2)
  add_test(
    NAME mol_render_wav_verify
    COMMAND
      ${CMAKE_COMMAND} -DWAV_PATH=${_mol_test_wav} -DREPORT_PATH=${_mol_test_report}
      -DFORMAT_HEX=0100 -DCHANNELS_HEX=0200 -DSAMPLE_RATE_HEX=80bb0000 -DBITS_HEX=1000
      -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_render.cmake)
  set_tests_properties(mol_render_wav_verify PROPERTIES DEPENDS mol_render_wav)

  set(_mol_sequence "${PROJECT_SOURCE_DIR}/examples/sequences/scale-study.molseq")
  set(_mol_sequence_wav "${CMAKE_CURRENT_BINARY_DIR}/test-sequence.wav")
  set(_mol_sequence_report "${CMAKE_CURRENT_BINARY_DIR}/test-sequence.json")
  add_test(
    NAME mol_render_sequence
    COMMAND
      mol-render "${_mol_sequence}" --output "${_mol_sequence_wav}" --report
      "${_mol_sequence_report}" --tail 0.25)
  add_test(
    NAME mol_render_sequence_verify
    COMMAND
      ${CMAKE_COMMAND} -DWAV_PATH=${_mol_sequence_wav} -DREPORT_PATH=${_mol_sequence_report}
      -DFORMAT_HEX=0100 -DCHANNELS_HEX=0200 -DSAMPLE_RATE_HEX=80bb0000 -DBITS_HEX=1000
      -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_render.cmake)
  set_tests_properties(mol_render_sequence_verify PROPERTIES DEPENDS mol_render_sequence)
  set(_mol_sequence_repeat_wav "${CMAKE_CURRENT_BINARY_DIR}/test-sequence-repeat.wav")
  set(_mol_sequence_repeat_report "${CMAKE_CURRENT_BINARY_DIR}/test-sequence-repeat.json")
  add_test(
    NAME mol_render_sequence_repeat
    COMMAND
      mol-render "${_mol_sequence}" --output "${_mol_sequence_repeat_wav}" --report
      "${_mol_sequence_repeat_report}" --tail 0.25)
  add_test(
    NAME mol_render_sequence_determinism
    COMMAND
      ${CMAKE_COMMAND} -DFIRST=${_mol_sequence_wav} -DSECOND=${_mol_sequence_repeat_wav}
      -P ${PROJECT_SOURCE_DIR}/apps/mol-seq/verify_roundtrip.cmake)
  set_tests_properties(
    mol_render_sequence_determinism
    PROPERTIES DEPENDS "mol_render_sequence;mol_render_sequence_repeat")

  set(_mol_pcm24_wav "${CMAKE_CURRENT_BINARY_DIR}/test-pcm24-mono.wav")
  set(_mol_pcm24_report "${CMAKE_CURRENT_BINARY_DIR}/test-pcm24-mono.json")
  add_test(
    NAME mol_render_pcm24_mono
    COMMAND
      mol-render --output "${_mol_pcm24_wav}" --report "${_mol_pcm24_report}" --duration
      0.5 --gate 0.2 --sample-rate 22050 --channels 1 --format pcm24)
  add_test(
    NAME mol_render_pcm24_mono_verify
    COMMAND
      ${CMAKE_COMMAND} -DWAV_PATH=${_mol_pcm24_wav} -DREPORT_PATH=${_mol_pcm24_report}
      -DFORMAT_HEX=0100 -DCHANNELS_HEX=0100 -DSAMPLE_RATE_HEX=22560000 -DBITS_HEX=1800
      -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_render.cmake)
  set_tests_properties(mol_render_pcm24_mono_verify PROPERTIES DEPENDS mol_render_pcm24_mono)

  set(_mol_float_wav "${CMAKE_CURRENT_BINARY_DIR}/test-float-hq.wav")
  set(_mol_float_report "${CMAKE_CURRENT_BINARY_DIR}/test-float-hq.json")
  add_test(
    NAME mol_render_float_hq
    COMMAND
      mol-render "${_mol_sequence}" --output "${_mol_float_wav}" --report
      "${_mol_float_report}" --tail 0.25 --sample-rate 44100 --format float32 --quality high)
  add_test(
    NAME mol_render_float_hq_verify
    COMMAND
      ${CMAKE_COMMAND} -DWAV_PATH=${_mol_float_wav} -DREPORT_PATH=${_mol_float_report}
      -DFORMAT_HEX=0300 -DCHANNELS_HEX=0200 -DSAMPLE_RATE_HEX=44ac0000 -DBITS_HEX=2000
      -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_render.cmake)
  set_tests_properties(mol_render_float_hq_verify PROPERTIES DEPENDS mol_render_float_hq)
endif()
