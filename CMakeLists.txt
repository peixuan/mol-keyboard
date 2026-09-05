# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

project(
  mol_keyboard
  VERSION 0.1.0
  DESCRIPTION "Portable headless digital instrument engine"
  LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/MolOptions.cmake)
include(cmake/MolWarnings.cmake)
include(cmake/MolSanitizers.cmake)

# Keep Windows shared-library consumers beside mol_core.dll.  This is scoped to
# the opt-in shared configuration; the established static product layout stays
# unchanged.
if(WIN32 AND BUILD_SHARED_LIBS)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

add_library(mol_core src/c_api/mol_engine.c src/control/mol_wire.c src/dsp/mol_dsp.c src/effects/mol_effects.c
                     src/music/mol_music.c src/patch/mol_patch.c
                     src/patch/mol_builtin_patches.c src/sequence/mol_sequence.c
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
  # Preserve the same complete imported-target link contract for static and
  # shared configurations on ELF hosts.
  target_link_libraries(mol_core PUBLIC m)
endif()
set_target_properties(
  mol_core
  PROPERTIES C_EXTENSIONS OFF
             C_STANDARD_REQUIRED ON
             POSITION_INDEPENDENT_CODE ON
             EXPORT_NAME core
             VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR})
if(BUILD_SHARED_LIBS)
  target_compile_definitions(mol_core PRIVATE MOL_CORE_BUILDING_LIBRARY=1
                                      PUBLIC MOL_CORE_SHARED=1)
  set_target_properties(mol_core PROPERTIES C_VISIBILITY_PRESET hidden)
endif()
mol_enable_warnings(mol_core)
mol_enable_optional_instrumentation(mol_core)
# The public API constructs opaque engine objects in caller-owned byte storage.
# Keep that supported contract well-defined under whole-program C/C++ optimization.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mol_core PRIVATE -fno-strict-aliasing)
endif()

if(MOL_ENABLE_CHORUS)
  target_compile_definitions(mol_core PUBLIC MOL_ENABLE_CHORUS=1)
else()
  target_compile_definitions(mol_core PUBLIC MOL_ENABLE_CHORUS=0)
endif()
if(MOL_ENABLE_DELAY)
  target_compile_definitions(mol_core PUBLIC MOL_ENABLE_DELAY=1)
else()
  target_compile_definitions(mol_core PUBLIC MOL_ENABLE_DELAY=0)
endif()
if(MOL_ENABLE_REVERB)
  target_compile_definitions(mol_core PUBLIC MOL_ENABLE_REVERB=1)
else()
  target_compile_definitions(mol_core PUBLIC MOL_ENABLE_REVERB=0)
endif()

if(MOL_PROFILE_TINY)
  target_compile_definitions(
    mol_core PUBLIC MOL_BUILD_PROFILE=1 MOL_PROFILE_MAX_VOICES=12 MOL_PROFILE_SEQUENCE_EVENTS=256
                    MOL_PROFILE_PLUCK_FRAMES=384 MOL_PROFILE_CHORUS_FRAMES=1024
                    MOL_PROFILE_DELAY_FRAMES=8192 MOL_PROFILE_REVERB_FRAMES=8192
                    MOL_PROFILE_REVERB_SCALE_MILLI=650)
elseif(MOL_PROFILE_STANDARD)
  target_compile_definitions(
    mol_core PUBLIC MOL_BUILD_PROFILE=2 MOL_PROFILE_MAX_VOICES=32 MOL_PROFILE_SEQUENCE_EVENTS=2048
                    MOL_PROFILE_PLUCK_FRAMES=384 MOL_PROFILE_CHORUS_FRAMES=4096
                    MOL_PROFILE_DELAY_FRAMES=96000 MOL_PROFILE_REVERB_FRAMES=16384
                    MOL_PROFILE_REVERB_SCALE_MILLI=1000)
else()
  target_compile_definitions(
    mol_core PUBLIC MOL_BUILD_PROFILE=3 MOL_PROFILE_MAX_VOICES=64 MOL_PROFILE_SEQUENCE_EVENTS=4096
                    MOL_PROFILE_PLUCK_FRAMES=512 MOL_PROFILE_CHORUS_FRAMES=8192
                    MOL_PROFILE_DELAY_FRAMES=192000 MOL_PROFILE_REVERB_FRAMES=32768
                    MOL_PROFILE_REVERB_SCALE_MILLI=1350)
endif()

# This is an internal adapter with no public export surface.  Keep it static even
# when BUILD_SHARED_LIBS is used to exercise the supported shared mol_core ABI.
add_library(mol_platform_audio_runtime STATIC platforms/common/mol_audio_runtime.c)
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

if(MOL_BUILD_TOOLS OR MOL_BUILD_DAEMON OR MOL_BUILD_FUZZERS OR MOL_BUILD_NATIVE_DEBUG_GUI)
  add_library(mol_json STATIC apps/mol-seq/json.cpp)
  add_library(mol::json ALIAS mol_json)
  target_include_directories(mol_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/apps/mol-seq)
  target_compile_features(mol_json PUBLIC cxx_std_17)
  set_target_properties(mol_json PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_json)
  mol_enable_optional_instrumentation(mol_json)

  add_library(mol_sequence_document STATIC apps/mol-seq/sequence_document.cpp)
  add_library(mol::sequence_document ALIAS mol_sequence_document)
  target_include_directories(mol_sequence_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/apps/mol-seq)
  target_link_libraries(mol_sequence_document PUBLIC mol::core mol::json)
  target_compile_features(mol_sequence_document PUBLIC cxx_std_17)
  set_target_properties(mol_sequence_document PROPERTIES CXX_EXTENSIONS OFF
                                                         CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_sequence_document)
  mol_enable_optional_instrumentation(mol_sequence_document)
  if(WIN32)
    target_compile_definitions(mol_sequence_document PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  add_library(mol_midi STATIC apps/mol-seq/midi.cpp)
  add_library(mol::midi ALIAS mol_midi)
  target_include_directories(mol_midi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/apps/mol-seq)
  target_link_libraries(mol_midi PUBLIC mol::sequence_document)
  target_compile_features(mol_midi PUBLIC cxx_std_17)
  set_target_properties(mol_midi PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_midi)
  mol_enable_optional_instrumentation(mol_midi)

  add_library(mol_jsonrpc STATIC src/control/jsonrpc.cpp src/control/service_rpc.cpp)
  add_library(mol::jsonrpc ALIAS mol_jsonrpc)
  target_include_directories(mol_jsonrpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/control)
  target_link_libraries(mol_jsonrpc PUBLIC mol::json)
  target_compile_features(mol_jsonrpc PUBLIC cxx_std_17)
  set_target_properties(mol_jsonrpc PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_jsonrpc)
  mol_enable_optional_instrumentation(mol_jsonrpc)

  add_library(mol_service_control STATIC src/control/service_backend.cpp)
  add_library(mol::service_control ALIAS mol_service_control)
  target_include_directories(mol_service_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/control)
  target_link_libraries(mol_service_control PUBLIC mol::jsonrpc mol::sequence_document)
  target_compile_features(mol_service_control PUBLIC cxx_std_17)
  set_target_properties(mol_service_control PROPERTIES CXX_EXTENSIONS OFF
                                                       CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_service_control)
  mol_enable_optional_instrumentation(mol_service_control)

  find_package(Threads REQUIRED)
  add_library(
    mol_local_ipc STATIC src/control/local_ipc.cpp src/control/loopback_websocket.cpp
                         src/control/service_paths.cpp)
  add_library(mol::local_ipc ALIAS mol_local_ipc)
  target_include_directories(mol_local_ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/control)
  target_compile_features(mol_local_ipc PUBLIC cxx_std_17)
  target_link_libraries(mol_local_ipc PUBLIC Threads::Threads)
  set_target_properties(mol_local_ipc PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_enable_warnings(mol_local_ipc)
  mol_enable_optional_instrumentation(mol_local_ipc)
  if(WIN32)
    target_compile_definitions(mol_local_ipc PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  if(WIN32)
    target_link_libraries(mol_local_ipc PRIVATE ws2_32)
  endif()
  if(MOL_BUILD_FUZZERS AND WIN32)
    target_compile_definitions(
      mol_json PUBLIC _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION
                      _ITERATOR_DEBUG_LEVEL=0)
    target_compile_definitions(
      mol_jsonrpc PUBLIC _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION
                         _ITERATOR_DEBUG_LEVEL=0)
    target_compile_definitions(
      mol_sequence_document PUBLIC _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION
                                   _ITERATOR_DEBUG_LEVEL=0)
    target_compile_definitions(
      mol_midi PUBLIC _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION
                      _ITERATOR_DEBUG_LEVEL=0)
    target_compile_definitions(
      mol_service_control PUBLIC _DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION
                                 _ITERATOR_DEBUG_LEVEL=0)
  endif()
endif()

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
configure_package_config_file(
  cmake/mol_keyboardConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/mol_keyboardConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mol_keyboard)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/mol_keyboardConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/mol_keyboardConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/mol_keyboardConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mol_keyboard)

install(
  FILES README.md CHANGELOG.md LICENSE NOTICE PRIVACY.md THIRD_PARTY_NOTICES.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/docs)
install(DIRECTORY third_party/licenses/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/licenses)
install(DIRECTORY patches/builtin/ patches/compiled/ patches/schema/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/patches)
install(DIRECTORY examples/sequences/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/examples/sequences)
install(FILES sbom/mol-keyboard.spdx.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/sbom)

if(MOL_PACKAGE_WEB_ASSETS)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/web/dist/index.html")
    message(FATAL_ERROR
            "MOL_PACKAGE_WEB_ASSETS requires a completed apps/web production build")
  endif()
  install(DIRECTORY apps/web/dist/
          DESTINATION ${CMAKE_INSTALL_DATADIR}/mol-keyboard/web)
endif()

if(EMSCRIPTEN AND DEFINED ENV{EMSDK_NODE})
  set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{EMSDK_NODE}")
endif()

if(MOL_BUILD_TESTS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
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
  add_subdirectory(apps/mol-seq)
  add_subdirectory(apps/mol-render)
  add_subdirectory(tools/audio-analyze)
  add_subdirectory(tools/latency-probe)
endif()

if(MOL_BUILD_DESKTOP_GUI OR MOL_BUILD_NATIVE_DEBUG_GUI)
  include(cmake/MolDependencies.cmake)
  mol_add_wxwidgets(${MOL_BUILD_DESKTOP_GUI})
endif()

if((MOL_BUILD_WEB_SERVER OR MOL_BUILD_DESKTOP_GUI OR MOL_BUILD_NATIVE_DEBUG_GUI) AND
   (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
    (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")))
  add_subdirectory(apps/mol-keyboard)
endif()

set(_mol_desktop_audio_available OFF)
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  set(_mol_desktop_audio_available ON)
endif()
if((MOL_BUILD_DESKTOP_AUDIO OR MOL_BUILD_DAEMON) AND _mol_desktop_audio_available)
  include(cmake/MolDependencies.cmake)
  mol_add_miniaudio()
  if(MOL_BUILD_DESKTOP_AUDIO)
    add_subdirectory(apps/mol-play)
  endif()
  if(MOL_BUILD_DAEMON)
    add_subdirectory(apps/mol-keyboardd)
    add_subdirectory(apps/molctl)
  endif()
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
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_subdirectory(platforms/ios)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  add_subdirectory(platforms/harmony)
elseif(MOL_VALIDATE_MOBILE_SOURCES AND NOT CMAKE_CROSSCOMPILING)
  add_subdirectory(platforms/harmony)
endif()

set(CPACK_PACKAGE_NAME "mol-keyboard")
set(CPACK_PACKAGE_VENDOR "MoL Keyboard contributors")
set(CPACK_PACKAGE_CONTACT "MoL Keyboard contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Portable headless digital instrument engine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_FILE_NAME
    "mol-keyboard-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_THREADS 0)
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
