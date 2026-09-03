# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

project(mol_installed_package_consumer LANGUAGES C CXX)

find_package(mol_keyboard 0.1 CONFIG REQUIRED)
if(NOT TARGET mol::core)
  message(FATAL_ERROR "The installed package did not provide mol::core")
endif()

function(mol_consumer_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_executable(mol_installed_c_consumer ../consumers/c_consumer.c)
target_link_libraries(mol_installed_c_consumer PRIVATE mol::core)
target_compile_features(mol_installed_c_consumer PRIVATE c_std_11)
set_target_properties(mol_installed_c_consumer PROPERTIES C_EXTENSIONS OFF
                                                          C_STANDARD_REQUIRED ON)
mol_consumer_warnings(mol_installed_c_consumer)

add_executable(mol_installed_cpp_consumer ../consumers/cpp_consumer.cpp)
target_link_libraries(mol_installed_cpp_consumer PRIVATE mol::core)
target_compile_features(mol_installed_cpp_consumer PRIVATE cxx_std_17)
set_target_properties(mol_installed_cpp_consumer PROPERTIES CXX_EXTENSIONS OFF
                                                            CXX_STANDARD_REQUIRED ON)
mol_consumer_warnings(mol_installed_cpp_consumer)

get_target_property(_mol_core_type mol::core TYPE)
if(WIN32 AND _mol_core_type STREQUAL "SHARED_LIBRARY")
  foreach(_consumer mol_installed_c_consumer mol_installed_cpp_consumer)
    add_custom_command(
      TARGET ${_consumer}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mol::core>
              $<TARGET_FILE_DIR:${_consumer}>
      VERBATIM)
  endforeach()
endif()

# Every installed public header must be self-contained in both supported source
# languages. Separate translation units prevent one header from masking another
# header's missing includes or C++ compatibility problems.
set(_mol_public_headers
    capabilities.h
    command.h
    effects.h
    engine.h
    event.h
    export.h
    mol.h
    music.h
    patch.h
    result.h
    sequence.h
    transport.h
    version.h
    wire.h)
foreach(_header IN LISTS _mol_public_headers)
  string(REPLACE "." "_" _header_id "${_header}")
  set(_c_source "${CMAKE_CURRENT_BINARY_DIR}/header_${_header_id}.c")
  set(_cpp_source "${CMAKE_CURRENT_BINARY_DIR}/header_${_header_id}.cpp")
  file(WRITE "${_c_source}"
       "#include <mol/${_header}>\nint mol_header_${_header_id}_c(void) { return 0; }\n")
  file(WRITE "${_cpp_source}"
       "#include <mol/${_header}>\nint mol_header_${_header_id}_cpp() { return 0; }\n")

  set(_c_target "mol_installed_header_c_${_header_id}")
  set(_cpp_target "mol_installed_header_cpp_${_header_id}")
  add_library(${_c_target} OBJECT "${_c_source}")
  target_link_libraries(${_c_target} PRIVATE mol::core)
  target_compile_features(${_c_target} PRIVATE c_std_11)
  set_target_properties(${_c_target} PROPERTIES C_EXTENSIONS OFF C_STANDARD_REQUIRED ON)
  mol_consumer_warnings(${_c_target})

  add_library(${_cpp_target} OBJECT "${_cpp_source}")
  target_link_libraries(${_cpp_target} PRIVATE mol::core)
  target_compile_features(${_cpp_target} PRIVATE cxx_std_17)
  set_target_properties(${_cpp_target} PROPERTIES CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
  mol_consumer_warnings(${_cpp_target})
endforeach()

enable_testing()
add_test(NAME mol_installed_c_consumer COMMAND mol_installed_c_consumer)
add_test(NAME mol_installed_cpp_consumer COMMAND mol_installed_cpp_consumer)
