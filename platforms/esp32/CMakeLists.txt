# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
option(MOL_DEVICE_WEB_COMPONENTS "Include the optional device Web service components" OFF)
if(MOL_DEVICE_WEB_COMPONENTS)
  set(COMPONENTS main esp_event esp_http_server esp_netif esp_wifi)
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(NOT MOL_DEVICE_WEB_COMPONENTS)
  idf_build_set_property(MINIMAL_BUILD ON)
endif()
project(mol_keyboard_esp32)
