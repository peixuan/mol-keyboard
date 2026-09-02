# SPDX-License-Identifier: Apache-2.0

if(ANDROID)
  add_library(mol_android_audio SHARED native/android_audio_host.cpp
                                       native/jni_bridge.cpp)
  target_link_libraries(mol_android_audio PRIVATE mol::platform_audio_runtime
                                                  mol::oboe)
  set_target_properties(mol_android_audio PROPERTIES OUTPUT_NAME mol_android_audio)
  install(TARGETS mol_android_audio LIBRARY DESTINATION lib/${ANDROID_ABI})
  set(_mol_android_target mol_android_audio)
else()
  add_library(mol_android_audio_source_check OBJECT native/android_audio_host.cpp
                                                    native/jni_bridge.cpp)
  target_include_directories(mol_android_audio_source_check
                             PRIVATE source_check/include)
  target_link_libraries(mol_android_audio_source_check
                        PRIVATE mol::platform_audio_runtime mol::oboe_headers)
  set(_mol_android_target mol_android_audio_source_check)
endif()

target_compile_features(${_mol_android_target} PRIVATE cxx_std_17)

if(ANDROID)
  mol_enable_warnings(mol_android_audio)
else()
  mol_enable_warnings(mol_android_audio_source_check)
endif()
