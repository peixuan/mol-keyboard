# SPDX-License-Identifier: Apache-2.0

foreach(_required MOL_SOURCE_DIR MOL_BINARY_DIR MOL_GENERATOR MOL_C_COMPILER
                  MOL_CXX_COMPILER MOL_CTEST_COMMAND)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

get_filename_component(_binary_dir "${MOL_BINARY_DIR}" ABSOLUTE)
if(_binary_dir STREQUAL "/" OR _binary_dir MATCHES "^[A-Za-z]:/$")
  message(FATAL_ERROR "Refusing to use a filesystem root as MOL_BINARY_DIR")
endif()

set(_gate_root "${_binary_dir}/installed-package-gate")
set(_prefix "${_gate_root}/prefix")
set(_consumer_build "${_gate_root}/consumer")
file(REMOVE_RECURSE "${_gate_root}")

set(_install_command "${CMAKE_COMMAND}" --install "${_binary_dir}" --prefix "${_prefix}")
if(DEFINED MOL_CONFIG AND NOT MOL_CONFIG STREQUAL "")
  list(APPEND _install_command --config "${MOL_CONFIG}")
endif()
execute_process(COMMAND ${_install_command} RESULT_VARIABLE _install_result)
if(NOT _install_result EQUAL 0)
  message(FATAL_ERROR "Installing the package failed with exit code ${_install_result}")
endif()

file(GLOB_RECURSE _installed_files LIST_DIRECTORIES FALSE "${_prefix}/*")
list(FILTER _installed_files INCLUDE REGEX "[/\\]mol_keyboardConfig[.]cmake$")
list(LENGTH _installed_files _package_config_count)
if(NOT _package_config_count EQUAL 1)
  message(FATAL_ERROR
          "Expected exactly one installed mol_keyboardConfig.cmake, found ${_package_config_count}")
endif()
list(GET _installed_files 0 _package_config)
get_filename_component(_package_config_dir "${_package_config}" DIRECTORY)

set(_configure_command
    "${CMAKE_COMMAND}"
    -S "${MOL_SOURCE_DIR}/tests/package-consumer"
    -B "${_consumer_build}"
    -G "${MOL_GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    "-Dmol_keyboard_DIR=${_package_config_dir}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    "-DCMAKE_C_COMPILER=${MOL_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${MOL_CXX_COMPILER}")
if(DEFINED MOL_BUILD_TYPE AND NOT MOL_BUILD_TYPE STREQUAL "")
  list(APPEND _configure_command "-DCMAKE_BUILD_TYPE=${MOL_BUILD_TYPE}")
endif()
if(DEFINED MOL_MAKE_PROGRAM AND NOT MOL_MAKE_PROGRAM STREQUAL "")
  list(APPEND _configure_command "-DCMAKE_MAKE_PROGRAM=${MOL_MAKE_PROGRAM}")
endif()
execute_process(COMMAND ${_configure_command} RESULT_VARIABLE _configure_result)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
          "Configuring the installed-package consumers failed with exit code ${_configure_result}")
endif()

set(_build_command "${CMAKE_COMMAND}" --build "${_consumer_build}")
if(DEFINED MOL_CONFIG AND NOT MOL_CONFIG STREQUAL "")
  list(APPEND _build_command --config "${MOL_CONFIG}")
endif()
execute_process(COMMAND ${_build_command} RESULT_VARIABLE _build_result)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
          "Building the installed-package consumers failed with exit code ${_build_result}")
endif()

set(_test_command "${MOL_CTEST_COMMAND}" --test-dir "${_consumer_build}" --output-on-failure)
if(DEFINED MOL_CONFIG AND NOT MOL_CONFIG STREQUAL "")
  list(APPEND _test_command -C "${MOL_CONFIG}")
endif()
execute_process(COMMAND ${_test_command} RESULT_VARIABLE _test_result)
if(NOT _test_result EQUAL 0)
  message(FATAL_ERROR
          "Running the installed-package consumers failed with exit code ${_test_result}")
endif()
