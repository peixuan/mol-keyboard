# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

project(
  mol_keyboard
  VERSION 0.1.0
  DESCRIPTION "Portable headless digital instrument engine"
  LANGUAGES C CXX)

include(GNUInstallDirs)
include(cmake/MolOptions.cmake)
include(cmake/MolWarnings.cmake)
include(cmake/MolSanitizers.cmake)

add_library(mol_core src/c_api/mol_engine.c src/dsp/mol_dsp.c src/effects/mol_effects.c
                     src/music/mol_music.c src/patch/mol_patch.c
                     src/patch/mol_builtin_patches.c
                     src/patch/generated/mol_patch_grand_piano.c
                     src/patch/generated/mol_patch_electric_piano.c
                     src/patch/generated/mol_patch_harpsichord.c
                     src/patch/generated/mol_patch_church_organ.c
                     src/patch/generated/mol_patch_jazz_organ.c
                     src/patch/generated/mol_patch_nylon_guitar.c
                     src/patch/generated/mol_patch_steel_guitar.c
                     src/patch/generated/mol_patch_violin.c
                     src/patch/generated/mol_patch_cello.c
                     src/patch/generated/mol_patch_flute.c
                     src/patch/generated/mol_patch_clarinet.c
                     src/patch/generated/mol_patch_synth_lead.c
                     src/patch/generated/mol_patch_synth_pad.c
                     src/patch/generated/mol_patch_synth_bass.c
                     src/patch/generated/mol_patch_choir.c
                     src/patch/generated/mol_patch_vibraphone.c
                     src/patch/generated/mol_patch_harp.c
                     src/patch/generated/mol_patch_music_box.c
                     src/transport/mol_transport.c)
add_library(mol::core ALIAS mol_core)

target_include_directories(
  mol_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/effects)
target_compile_features(mol_core PUBLIC c_std_11)
if(UNIX AND NOT APPLE)
  target_link_libraries(mol_core PRIVATE m)
endif()
set_target_properties(
  mol_core
  PROPERTIES C_EXTENSIONS OFF
             C_STANDARD_REQUIRED ON
             POSITION_INDEPENDENT_CODE ON
             VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             WINDOWS_EXPORT_ALL_SYMBOLS ON)
mol_enable_warnings(mol_core)
mol_enable_optional_instrumentation(mol_core)

if(MOL_PROFILE_TINY)
  target_compile_definitions(
    mol_core PUBLIC MOL_BUILD_PROFILE=1 MOL_PROFILE_MAX_VOICES=12
                    MOL_PROFILE_PLUCK_FRAMES=384)
elseif(MOL_PROFILE_STANDARD)
  target_compile_definitions(
    mol_core PUBLIC MOL_BUILD_PROFILE=2 MOL_PROFILE_MAX_VOICES=32
                    MOL_PROFILE_PLUCK_FRAMES=384)
else()
  target_compile_definitions(
    mol_core PUBLIC MOL_BUILD_PROFILE=3 MOL_PROFILE_MAX_VOICES=64
                    MOL_PROFILE_PLUCK_FRAMES=512)
endif()

add_library(mol_platform_audio_runtime platforms/common/mol_audio_runtime.c)
add_library(mol::platform_audio_runtime ALIAS mol_platform_audio_runtime)
target_include_directories(
  mol_platform_audio_runtime
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platforms/common/include>)
target_compile_features(mol_platform_audio_runtime PUBLIC c_std_11)
target_link_libraries(mol_platform_audio_runtime PUBLIC mol::core)
set_target_properties(
  mol_platform_audio_runtime
  PROPERTIES C_EXTENSIONS OFF
             C_STANDARD_REQUIRED ON
             POSITION_INDEPENDENT_CODE ON)
mol_enable_warnings(mol_platform_audio_runtime)
mol_enable_optional_instrumentation(mol_platform_audio_runtime)

install(
  TARGETS mol_core
  EXPORT mol_keyboardTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT mol_keyboardTargets
  NAMESPACE mol::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mol_keyboard)

if(EMSCRIPTEN AND DEFINED ENV{EMSDK_NODE})
  set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{EMSDK_NODE}")
endif()

if(MOL_BUILD_TESTS)
  enable_testing()
  add_test(
    NAME mol_dependency_license_audit
    COMMAND
      ${CMAKE_COMMAND} -DMOL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/license_audit.cmake)
endif()

if(EMSCRIPTEN)
  add_subdirectory(platforms/web/emscripten)
endif()

if(MOL_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(MOL_BUILD_TOOLS)
  add_subdirectory(apps/mol-patchc)
  add_subdirectory(apps/mol-render)
endif()

set(_mol_desktop_audio_available OFF)
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  set(_mol_desktop_audio_available ON)
endif()
if(MOL_BUILD_DESKTOP_AUDIO AND _mol_desktop_audio_available)
  include(cmake/MolDependencies.cmake)
  mol_add_miniaudio()
  add_subdirectory(apps/mol-play)
endif()

if(ANDROID)
  include(cmake/MolDependencies.cmake)
  mol_add_oboe()
  add_subdirectory(platforms/android)
elseif(MOL_VALIDATE_MOBILE_SOURCES AND NOT CMAKE_CROSSCOMPILING)
  include(cmake/MolDependencies.cmake)
  mol_add_oboe_headers()
  add_subdirectory(platforms/android)
endif()

if(APPLE)
  add_subdirectory(platforms/apple)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  add_subdirectory(platforms/harmony)
elseif(MOL_VALIDATE_MOBILE_SOURCES AND NOT CMAKE_CROSSCOMPILING)
  add_subdirectory(platforms/harmony)
endif()
