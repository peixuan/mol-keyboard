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

add_library(mol_core src/c_api/mol_engine.c)
add_library(mol::core ALIAS mol_core)

target_include_directories(
  mol_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
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
  target_compile_definitions(mol_core PUBLIC MOL_BUILD_PROFILE=1 MOL_PROFILE_MAX_VOICES=12)
elseif(MOL_PROFILE_STANDARD)
  target_compile_definitions(mol_core PUBLIC MOL_BUILD_PROFILE=2 MOL_PROFILE_MAX_VOICES=32)
else()
  target_compile_definitions(mol_core PUBLIC MOL_BUILD_PROFILE=3 MOL_PROFILE_MAX_VOICES=64)
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

if(MOL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MOL_BUILD_TOOLS)
  add_subdirectory(apps/mol-render)
endif()
