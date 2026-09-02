# SPDX-License-Identifier: Apache-2.0

enable_language(OBJCXX)

add_library(mol_apple_audio STATIC MOLAppleAudioHost.mm)
target_include_directories(mol_apple_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(mol_apple_audio PRIVATE cxx_std_17)
target_compile_options(mol_apple_audio PRIVATE
                       $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
set_target_properties(
  mol_apple_audio
  PROPERTIES OBJCXX_STANDARD 17
             OBJCXX_STANDARD_REQUIRED ON
             OBJCXX_EXTENSIONS OFF)
target_link_libraries(mol_apple_audio PRIVATE mol::platform_audio_runtime
                                              "-framework AudioToolbox"
                                              "-framework Foundation")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  target_link_libraries(mol_apple_audio PRIVATE "-framework AVFAudio")
endif()
mol_enable_warnings(mol_apple_audio)

install(TARGETS mol_apple_audio ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES MOLAppleAudioHost.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mol/apple)
