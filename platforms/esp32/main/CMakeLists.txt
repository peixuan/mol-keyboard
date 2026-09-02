# SPDX-License-Identifier: Apache-2.0

idf_component_register(
  SRCS "main.c"
  REQUIRES mol_core esp_driver_i2s esp_system esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
