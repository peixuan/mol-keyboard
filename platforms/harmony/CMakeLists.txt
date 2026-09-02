# SPDX-License-Identifier: Apache-2.0

set(_mol_harmony_sources native/oh_audio_host.cpp native/napi_module.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  add_library(mol_harmony_audio SHARED ${_mol_harmony_sources})
  target_link_libraries(mol_harmony_audio PRIVATE mol::platform_audio_runtime
                                                  libace_napi.z.so libohaudio.so)
  set_target_properties(mol_harmony_audio PROPERTIES OUTPUT_NAME mol_harmony_audio)
  install(TARGETS mol_harmony_audio LIBRARY DESTINATION lib)
  set(_mol_harmony_target mol_harmony_audio)
else()
  add_library(mol_harmony_audio_source_check OBJECT ${_mol_harmony_sources})
  target_include_directories(mol_harmony_audio_source_check
                             PRIVATE source_check/include)
  target_link_libraries(mol_harmony_audio_source_check
                        PRIVATE mol::platform_audio_runtime)
  set(_mol_harmony_target mol_harmony_audio_source_check)
endif()

target_compile_features(${_mol_harmony_target} PRIVATE cxx_std_17)
mol_enable_warnings(${_mol_harmony_target})
