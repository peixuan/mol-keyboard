# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(mol_keyboard_esp32)
